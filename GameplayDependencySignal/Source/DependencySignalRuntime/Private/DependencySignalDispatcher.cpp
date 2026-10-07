// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalDispatcher.h"

#include "DependencySignalRuntime.h"
#include "Algo/AllOf.h"

namespace DependencySignal::Private
{
	/**
	 * Whether a listener can be dropped for good.
	 *
	 * A callback bound to a UObject reports IsBound() == false as soon as that object is marked as garbage,
	 * so for those we wait until the object has actually been collected. Other bindings (SP, raw, static,
	 * lambda) or an empty delegate can never become bound again, so an unbound delegate means dead.
	 */
	bool IsListenerDead(const bool bHasOwner, const FWeakObjectPtr& Owner, const FDependencyStateEvent& Event)
	{
		return bHasOwner ? Owner.IsStale(/* bIncludingGarbage */ false) : !Event.IsBound();
	}
} // namespace DependencySignal::Private

FDependencySignalDispatcher::FDependencySignalDispatcher(
	const FGameplayTagContainer& InRootTags, const UObject* InWorldContext)
	: RootTags(InRootTags)
	, WorldContext(InWorldContext)
{
}

FDependencySignalDispatcher::~FDependencySignalDispatcher()
{
	Reset();
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::Register(FDependencySignalRequest&& Request)
{
	if (!IsValidDependencyTags(Request.DependencyTags))
	{
		DS_UE_LOG(Warning, TEXT("The operation failed due to invalid tags [%s]"), *Request.DependencyTags.ToString());
		return FListenerHandle();
	}

	if (Request.bAnySource)
	{
		return RegisterAnyDependencyContainer(MoveTemp(Request));
	}

	return RegisterDependencyContainer(Request, Request.SourceObject.Get(), /* bCreateIfDoesntExist */ true);
}

void FDependencySignalDispatcher::Unregister(FListenerHandle& ListenerHandle)
{
	const FListenerHandle TempHandle = ListenerHandle;
	ListenerHandle.Reset();

	if (HandleToListener.Remove(TempHandle))
	{
		return;
	}

	if (TUniquePtr<FAnyDepListenerEntry> const* Listener = AnyListenerByHandle.Find(TempHandle))
	{
		// Snapshot the per-source handles and the origin entry before mutating any map:
		// Listener points into AnyListenerByHandle, which we edit below.
		const TArray<FListenerHandle> PerSourceHandles = Listener->Get()->Handles;
		const TSharedPtr<FAnyDependencyEntry> Entry = Listener->Get()->Entry.Pin();

		// The per-source listeners generated for this any-source registration live in
		// HandleToListener; remove them so they stop firing on future Validate() calls.
		for (const FListenerHandle& Handle : PerSourceHandles)
		{
			HandleToListener.Remove(Handle);
		}

		if (Entry.IsValid())
		{
			for (auto It = Entry->Request.DependencyTags.CreateConstIterator(); It; ++It)
			{
				if (TUniquePtr<FAnyDependencyContainer> const* Container = AnyDependencyByTag.Find(*It);
					Container && Container->IsValid())
				{
					Container->Get()->Entries.Remove(Entry);
				}
			}
		}

		AnyListenerByHandle.Remove(TempHandle);
	}
}

bool FDependencySignalDispatcher::IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject) const
{
	const FDepEntry* Entry = GetDependencyEntry(SourceObject, Tag);
	return Entry && Entry->IsHierarchyValid();
}

bool FDependencySignalDispatcher::IsValidDependency(
	const FGameplayTagContainer& Tags, const UObject* SourceObject) const
{
	return AreDependenciesValid(FindDependencyContainer(SourceObject), Tags);
}

bool FDependencySignalDispatcher::AreDependenciesValid(
	const FDepContainer* Container, const FGameplayTagContainer& Tags) const
{
	return Algo::AllOf(Tags, [Container](const FGameplayTag& Tag) {
		const FDepEntry* Entry = Container ? Container->FindEntry(Tag) : nullptr;
		return Entry && Entry->IsHierarchyValid();
	});
}

int32 FDependencySignalDispatcher::GetListenersNum() const
{
	return HandleToListener.Num();
}

int32 FDependencySignalDispatcher::GetAnyListenersNum() const
{
	return AnyListenerByHandle.Num();
}

void FDependencySignalDispatcher::Validate(
	const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::Validate);

	FDepEntry* Entry = GetDependencyEntry(SourceObject, Tag, /* bCreateIfDoesntExist */ true);
	check(Entry != nullptr);

	ValidateAnyDependency(Tag, SourceObject, bForceRetrigger);

	if (bForceRetrigger || Entry->State.bIsValid != bIsValid)
	{
		Entry->State.bIsValid = bIsValid;

		if (bForceRetrigger || Entry->State.bIsValidParent)
		{
			HandleDependencyStateChanges(Entry, SourceObject, bForceRetrigger);
		}

		ShareDependencyState(Entry, SourceObject, bForceRetrigger);
	}
}

void FDependencySignalDispatcher::Reset()
{
	SourceToDependency.Empty();
	HandleToListener.Empty();
	AnyDependencyByTag.Empty();
	AnyListenerByHandle.Empty();
}

bool FDependencySignalDispatcher::IsValidDependencyTags(const FGameplayTagContainer& Tags) const
{
	return Algo::AllOf(Tags, [this](const FGameplayTag& Tag) {
		return IsValidDependencyTag(Tag);
	});
}

bool FDependencySignalDispatcher::IsValidDependencyTag(const FGameplayTag& Tag) const
{
	if (RootTags.IsEmpty())
	{
		return true;
	}

	return ensureAlwaysMsgf(
		Tag.MatchesAny(RootTags), TEXT("Tag must be inherited from any of [%s] tags"), *RootTags.ToStringSimple());
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::GenerateNewListenerHandle()
{
	return FListenerHandle(++HandleIdCounter, AsShared());
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::RegisterDependencyContainer(
	const FDependencySignalRequest& Request, const UObject* SourceObject, const bool bCreateIfDoesntExist)
{
	if (!IsValidDependencyTags(Request.DependencyTags))
	{
		DS_UE_LOG(Warning, TEXT("The operation failed due to invalid tags [%s]"), *Request.DependencyTags.ToString());
		return FListenerHandle();
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::RegisterDependencyContainer);

	const FListenerHandle& NewHandle = GenerateNewListenerHandle();
	check(NewHandle.IsValid());

	TUniquePtr<FDepListenerEntry> NewListenerEntry = MakeUnique<FDepListenerEntry>();
	NewListenerEntry->DependencyTags = Request.DependencyTags;
	NewListenerEntry->Event_StateChanged = Request.OnDependencyStateChanged;

	if (const UObject* Owner = Request.OnDependencyStateChanged.GetUObject())
	{
		NewListenerEntry->Owner = FWeakObjectPtr(Owner);
		NewListenerEntry->bHasOwner = true;
	}

	HandleToListener.Add(NewHandle, MoveTemp(NewListenerEntry));

	bool bIsHierarchyDependencyValid = true;
	for (const FGameplayTag& DependencyTag : Request.DependencyTags)
	{
		FDepEntry* DependencyEntry = GetDependencyEntry(SourceObject, DependencyTag, bCreateIfDoesntExist);
		if (!DependencyEntry)
		{
			bIsHierarchyDependencyValid = false;
			continue;
		}

		DependencyEntry->ListenerHandles.Add(NewHandle);

		bIsHierarchyDependencyValid &= DependencyEntry->IsHierarchyValid();
	}

	const TUniquePtr<FDepListenerEntry>& ListenerEntry = HandleToListener[NewHandle];
	check(ListenerEntry.IsValid());

	ListenerEntry->bWasValid = bIsHierarchyDependencyValid;
	ListenerEntry->Event_StateChanged.ExecuteIfBound(
		bIsHierarchyDependencyValid, GetSourceObjForBroadcast(SourceObject));

	return NewHandle;
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::RegisterAnyDependencyContainer(
	FDependencySignalRequest&& Request)
{
	const FListenerHandle& NewHandle = GenerateNewListenerHandle();
	check(NewHandle.IsValid());

	const TSharedPtr<FAnyDependencyEntry> NewEntry = MakeShared<FAnyDependencyEntry>();
	NewEntry->Request = MoveTemp(Request);
	NewEntry->RegisteredSources.Reserve(SourceToDependency.Num());
	NewEntry->AnyListenerHandle = NewHandle;

	if (const UObject* Owner = NewEntry->Request.OnDependencyStateChanged.GetUObject())
	{
		NewEntry->Owner = FWeakObjectPtr(Owner);
		NewEntry->bHasOwner = true;
	}

	// `Request` was moved into NewEntry above and is empty from here on: always use NewEntry->Request.
	const FDependencySignalRequest& AnyRequest = NewEntry->Request;

	TUniquePtr<FAnyDepListenerEntry> NewListenerEntry = MakeUnique<FAnyDepListenerEntry>();
	NewListenerEntry->Handles.Reserve(SourceToDependency.Num());
	NewListenerEntry->Entry = NewEntry;

	// Collect sources first: registering creates entries inside the containers we would otherwise be iterating.
	TArray<const UObject*, TInlineAllocator<16>> ExistingSources;
	ExistingSources.Reserve(SourceToDependency.Num());
	for (const TPair<FObjectKey, TUniquePtr<FDepContainer>>& SourceDependency : SourceToDependency)
	{
		if (const UObject* SourceObject = SourceDependency.Key.ResolveObjectPtr(); IsValid(SourceObject))
		{
			ExistingSources.Add(SourceObject);
		}
	}

	for (const UObject* SourceObject : ExistingSources)
	{
		// Create missing entries so the listener is attached to every one of its tags for this source;
		// otherwise a tag validated later would never reach it (the source is already marked as registered).
		const FListenerHandle& Handle =
			RegisterDependencyContainer(AnyRequest, SourceObject, /* bCreateIfDoesntExist */ true);
		if (!Handle.IsValid())
		{
			continue;
		}

		NewEntry->RegisteredSources.Add(FObjectKey(SourceObject));
		NewListenerEntry->Handles.Add(Handle);
	}

	AnyListenerByHandle.Add(NewHandle, MoveTemp(NewListenerEntry));

	for (const FGameplayTag& Tag : NewEntry->Request.DependencyTags)
	{
		FAnyDependencyContainer* Container = GetAnyDependencyContainer(Tag, /* bCreateIfDoesntExist */ true);
		Container->Entries.Add(NewEntry);
	}

	return NewHandle;
}

FDependencySignalDispatcher::FDepContainer& FDependencySignalDispatcher::GetDependencyContainer(
	const UObject* SourceObject)
{
	// if SourceObject is nullptr,
	// we use World as SourceObject to have a dependency container for the local player client
	const UObject* RealSourceObject = IsValid(SourceObject) ? SourceObject : WorldContext.Get();

	if (!SourceToDependency.Contains(RealSourceObject))
	{
		SourceToDependency.Add(RealSourceObject, MakeUnique<FDepContainer>([this](FDepEntry& NewEntry) {
			InitDependencyEntry(NewEntry);
		}));
	}

	return *(*SourceToDependency.Find(RealSourceObject));
}

const FDependencySignalDispatcher::FDepContainer* FDependencySignalDispatcher::FindDependencyContainer(
	const UObject* SourceObject) const
{
	const UObject* RealSourceObject = IsValid(SourceObject) ? SourceObject : WorldContext.Get();

	if (const TUniquePtr<FDepContainer>* Container = SourceToDependency.Find(RealSourceObject))
	{
		return Container->Get();
	}

	return nullptr;
}

void FDependencySignalDispatcher::PruneStaleSources()
{
	for (auto It = SourceToDependency.CreateIterator(); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
	}

	// Listeners are never dropped on the notify path while their owner is merely garbage-flagged; this is where
	// they go once the owner is actually collected. Their handles left in FDepEntry::ListenerHandles are
	// cleaned lazily by HandleDependencyStateChanges.
	for (auto It = HandleToListener.CreateIterator(); It; ++It)
	{
		const FDepListenerEntry& Listener = *It.Value();
		if (DependencySignal::Private::IsListenerDead(Listener.bHasOwner, Listener.Owner, Listener.Event_StateChanged))
		{
			It.RemoveCurrent();
		}
	}

	// Each any-source registration owns exactly one FAnyDepListenerEntry, so iterating this map
	// prunes every FAnyDependencyEntry::RegisteredSources set once.
	TArray<FListenerHandle> DeadAnyHandles;
	for (const TPair<FListenerHandle, TUniquePtr<FAnyDepListenerEntry>>& Pair : AnyListenerByHandle)
	{
		const TSharedPtr<FAnyDependencyEntry> Entry = Pair.Value->Entry.Pin();
		if (!Entry.IsValid()
			|| DependencySignal::Private::IsListenerDead(
				Entry->bHasOwner, Entry->Owner, Entry->Request.OnDependencyStateChanged))
		{
			DeadAnyHandles.Add(Pair.Key);
			continue;
		}

		for (auto It = Entry->RegisteredSources.CreateIterator(); It; ++It)
		{
			if (!It->ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}

	// Unregister outside the loop above: it mutates AnyListenerByHandle.
	for (FListenerHandle& DeadHandle : DeadAnyHandles)
	{
		Unregister(DeadHandle);
	}
}

FDependencySignalDispatcher::FAnyDependencyContainer* FDependencySignalDispatcher::GetAnyDependencyContainer(
	const FGameplayTag& Tag, const bool bCreateIfDoesntExist)
{
	check(Tag.IsValid());

	if (const TUniquePtr<FAnyDependencyContainer>* Container = AnyDependencyByTag.Find(Tag))
	{
		return Container->Get();
	}

	return bCreateIfDoesntExist ? CreateAnyDependencyContainer(Tag) : nullptr;
}

FDependencySignalDispatcher::FDepEntry* FDependencySignalDispatcher::GetDependencyEntry(
	const UObject* SourceObject, const FGameplayTag& Tag, bool bCreateIfDoesntExist)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::GetDependencyEntry);

	FDepContainer& Container = GetDependencyContainer(SourceObject);

	if (FDepEntry* Entry = Container.FindEntry(Tag))
	{
		return Entry;
	}

	if (bCreateIfDoesntExist)
	{
		CreateDependencyHierarchy(Container, Tag);
		FDepEntry* Entry = Container.FindEntry(Tag);
		check(Entry != nullptr);
		return Entry;
	}

	return nullptr;
}

const FDependencySignalDispatcher::FDepEntry* FDependencySignalDispatcher::GetDependencyEntry(
	const UObject* SourceObject, const FGameplayTag& Tag) const
{
	// Read-only lookup: never create a container or entry as a side effect of a query.
	const FDepContainer* Container = FindDependencyContainer(SourceObject);
	return Container ? Container->FindEntry(Tag) : nullptr;
}

void FDependencySignalDispatcher::InitDependencyEntry(FDepEntry& NewEntry) const
{
	const bool bIsRootTag = RootTags.HasTagExact(NewEntry.Tag);

	NewEntry.State.bIsValid = bIsRootTag;
	NewEntry.State.bIsValidParent = bIsRootTag;
}

void FDependencySignalDispatcher::CreateDependencyHierarchy(FDepContainer& Container, const FGameplayTag& Tag) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::CreateDependencyHierarchy);

	check(Tag.IsValid());

	const FGameplayTagContainer& ParentTags = Tag.GetGameplayTagParents();
	const FGameplayTag& RootTag = ParentTags.Last();

	if (!Container.Contains(RootTag))
	{
		Container.AddEntry(RootTag);
	}

	FDepEntry* ParentEntry = Container.FindEntry(RootTag);
	check(ParentEntry != nullptr);

	// The Root tag (the last one) will be skipped because it was created earlier
	for (int32 Index = ParentTags.Num() - 2; Index >= 0; --Index)
	{
		const FGameplayTag& ChildTag = ParentTags.GetByIndex(Index);

		if (!Container.Contains(ChildTag))
		{
			FDepEntry* NewEntry = Container.AddEntry(ChildTag);
			NewEntry->State.bIsValidParent = ParentEntry->IsHierarchyValid();
			ParentEntry->ChildTags.AddTag(ChildTag);
			ParentEntry = NewEntry;
		}
		else
		{
			ParentEntry->ChildTags.AddTag(ChildTag);
			ParentEntry = Container.FindEntry(ChildTag);
		}
		check(ParentEntry != nullptr);
	}
}

FDependencySignalDispatcher::FAnyDependencyContainer* FDependencySignalDispatcher::CreateAnyDependencyContainer(
	const FGameplayTag& Tag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::CreateAnyDependencyContainer);

	check(Tag.IsValid());
	check(!AnyDependencyByTag.Contains(Tag));

	AnyDependencyByTag.Add(Tag, MakeUnique<FAnyDependencyContainer>(Tag));
	const FAnyDependencyContainer* ChildContainer = AnyDependencyByTag.Find(Tag)->Get();
	check(ChildContainer != nullptr);

	const FGameplayTagContainer& ParentTags = Tag.GetGameplayTagParents();

	// Skip the first tag because it created above.
	for (int32 Index = 1; Index < ParentTags.Num(); ++Index)
	{
		const FGameplayTag& ParentTag = ParentTags.GetByIndex(Index);

		const TUniquePtr<FAnyDependencyContainer>* ParentContainer = AnyDependencyByTag.Find(ParentTag);

		if (!ParentContainer)
		{
			AnyDependencyByTag.Add(ParentTag, MakeUnique<FAnyDependencyContainer>(ParentTag));
			ParentContainer = AnyDependencyByTag.Find(ParentTag);
		}

		ParentContainer->Get()->ChildTags.AddTag(ChildContainer->Tag);
		ChildContainer = ParentContainer->Get();
	}

	return AnyDependencyByTag.Find(Tag)->Get();
}

void FDependencySignalDispatcher::ShareDependencyState(
	FDepEntry* RootEntry, const UObject* SourceObject, const bool bForceRetrigger)
{
	check(RootEntry != nullptr);

	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::ShareDependencyState);

	TQueue<FDepEntry*, EQueueMode::SingleThreaded> Queue;
	Queue.Enqueue(RootEntry);

	for (FDepEntry* Entry; Queue.Dequeue(Entry);)
	{
		check(Entry != nullptr);

		const bool bIsValidParent = Entry->IsHierarchyValid();

		for (const FGameplayTag& Tag : Entry->ChildTags)
		{
			FDepEntry* DependencyEntry = GetDependencyEntry(SourceObject, Tag, /* bCreateIfDoesntExist */ true);
			check(DependencyEntry != nullptr);

			if (DependencyEntry->State.bIsValidParent == bIsValidParent)
			{
				continue;
			}

			DependencyEntry->State.bIsValidParent = bIsValidParent;

			if (!DependencyEntry->State.bIsValid)
			{
				continue;
			}

			HandleDependencyStateChanges(DependencyEntry, SourceObject, bForceRetrigger);

			Queue.Enqueue(DependencyEntry);
		}
	}
}

void FDependencySignalDispatcher::HandleDependencyStateChanges(
	FDepEntry* Entry, const UObject* SourceObject, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::HandleDependencyStateChanges);

	const FDepContainer* Container = FindDependencyContainer(SourceObject);

	for (int32 Index = Entry->ListenerHandles.Num() - 1; Index >= 0; --Index)
	{
		const FListenerHandle& ListenerHandle = Entry->ListenerHandles[Index];

		if (!HandleToListener.Contains(ListenerHandle))
		{
			Entry->ListenerHandles.RemoveAt(Index);
			continue;
		}

		const TUniquePtr<FDepListenerEntry>& ListenerEntry = HandleToListener[ListenerHandle];
		check(ListenerEntry.IsValid());

		if (!ListenerEntry->Event_StateChanged.IsBound())
		{
			// An unbound UObject callback only means its owner is garbage-flagged right now: skip it and let
			// the post-GC PruneStaleSources() drop it once the owner is collected. Anything else can never rebind.
			if (DependencySignal::Private::IsListenerDead(
					ListenerEntry->bHasOwner, ListenerEntry->Owner, ListenerEntry->Event_StateChanged))
			{
				Entry->ListenerHandles.RemoveAt(Index);
				HandleToListener.Remove(ListenerHandle);
			}
			else
			{
				DS_UE_LOG(Verbose, TEXT("Skipping listener %s for [%s]: callback owner is not safe to execute"),
					*ListenerHandle.ToString(), *Entry->Tag.ToString());
			}

			continue;
		}

		const bool bIsValidAllDependency = AreDependenciesValid(Container, ListenerEntry->DependencyTags);

		if (ListenerEntry->bWasValid != bIsValidAllDependency || bForceRetrigger)
		{
			ListenerEntry->bWasValid = bIsValidAllDependency;
			ListenerEntry->Event_StateChanged.ExecuteIfBound(
				bIsValidAllDependency, GetSourceObjForBroadcast(SourceObject));
		}
	}
}

void FDependencySignalDispatcher::ValidateAnyDependency(
	const FGameplayTag& Tag, const UObject* SourceObject, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::ValidateAnyDependency);

	FAnyDependencyContainer* Container = GetAnyDependencyContainer(Tag);
	if (!Container)
	{
		return;
	}

	SourceObject = IsValid(SourceObject) ? SourceObject : WorldContext.Get();
	check(SourceObject != nullptr);

	for (const TSharedPtr<FAnyDependencyEntry>& Entry : Container->Entries)
	{
		if (Entry->RegisteredSources.Contains(SourceObject))
		{
			continue;
		}

		// Create missing entries so a multi-tag listener is attached to all of its tags for this source,
		// not only to the one being validated now (the source is marked as registered below).
		const FListenerHandle& Handle =
			RegisterDependencyContainer(Entry->Request, SourceObject, /* bCreateIfDoesntExist */ true);

		if (!Handle.IsValid())
		{
			if (bForceRetrigger)
			{
				Entry->Request.OnDependencyStateChanged.ExecuteIfBound(false, GetSourceObjForBroadcast(SourceObject));
			}

			continue;
		}

		Entry->RegisteredSources.Add(SourceObject);

		AnyListenerByHandle.FindChecked(Entry->AnyListenerHandle).Get()->Handles.Add(Handle);
	}
}

UObject* FDependencySignalDispatcher::GetSourceObjForBroadcast(const UObject* SourceObject) const
{
	return SourceObject != WorldContext.Get() ? const_cast<UObject*>(SourceObject) : nullptr;
}