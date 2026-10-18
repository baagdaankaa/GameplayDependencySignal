// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalDispatcher.h"

#include "DependencySignalRuntime.h"
#include "Algo/AllOf.h"
#include "Algo/Sort.h"

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

	/**
	 * Orders tags so parents come before their children. When the tags are invalidated in this order, an invalid
	 * parent already reports its children as invalid, and the deeper tags are then switched off silently
	 * instead of producing more transitions.
	 */
	void SortParentsFirst(TArray<FGameplayTag>& Tags)
	{
		Algo::SortBy(Tags, [](const FGameplayTag& Tag) {
			return Tag.GetGameplayTagParents().Num();
		});
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::Register);
	check(IsInGameThread());

	if (Request.DependencyTags.IsEmpty())
	{
		DS_UE_LOG(Warning, TEXT("The operation failed: the request has no dependency tags"));
		return FListenerHandle();
	}

	if (!IsValidDependencyTags(Request.DependencyTags))
	{
		DS_UE_LOG(Warning, TEXT("The operation failed: invalid tags [%s]"), *Request.DependencyTags.ToString());
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::Unregister);
	check(IsInGameThread());

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
	check(IsInGameThread());

	FDepEntry* Entry = GetDependencyEntry(SourceObject, Tag, /* bCreateIfDoesntExist */ true);
	check(Entry != nullptr);

	// A direct writing to the local session takes the tag over: no link owner may invalidate it afterward.
	if (IsLocalSession(SourceObject))
	{
		LocalLinkOwners.Remove(Tag);
	}

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

void FDependencySignalDispatcher::ValidateLocalLink(
	const FGameplayTag& Tag, const bool bIsValid, const UObject* LinkOwner, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::ValidateLocalLink);
	check(IsInGameThread());

	const bool bHasLinkOwner = IsValid(LinkOwner);

	if (!bIsValid && bHasLinkOwner)
	{
		// Only the current owner may invalidate the link; anyone else was replaced or never published the tag.
		const FObjectKey* CurrentOwner = LocalLinkOwners.Find(Tag);
		if (!CurrentOwner || *CurrentOwner != FObjectKey(LinkOwner))
		{
			return;
		}
	}

	// Validate() drops the previous owner of a local session tag.
	Validate(Tag, bIsValid, /* SourceObject */ nullptr, bForceRetrigger);

	if (bIsValid && bHasLinkOwner)
	{
		LocalLinkOwners.Add(Tag, FObjectKey(LinkOwner));
	}
}

void FDependencySignalDispatcher::UnlinkLocalSource(const UObject* LinkOwner, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::UnlinkLocalSource);
	check(IsInGameThread());

	if (IsValid(LinkOwner))
	{
		UnlinkLocalTags(FObjectKey(LinkOwner), bForceRetrigger);
	}
}

void FDependencySignalDispatcher::InvalidateSource(const UObject* SourceObject, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::InvalidateSource);
	check(IsInGameThread());

	const FDepContainer* Container = FindDependencyContainer(SourceObject);
	if (!Container)
	{
		return;
	}

	TArray<FGameplayTag> Tags;
	for (const TPair<FGameplayTag, TUniquePtr<FDepEntry>>& Pair : Container->GetEntries())
	{
		if (Pair.Value.IsValid() && Pair.Value->State.bIsValid && !RootTags.HasTagExact(Pair.Key))
		{
			Tags.Add(Pair.Key);
		}
	}

	InvalidateTags(MoveTemp(Tags), SourceObject, bForceRetrigger);
}

UObject* FDependencySignalDispatcher::GetLocalLinkOwner(const FGameplayTag& Tag) const
{
	const FObjectKey* Owner = LocalLinkOwners.Find(Tag);
	return Owner ? Owner->ResolveObjectPtr() : nullptr;
}

void FDependencySignalDispatcher::Reset()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::Reset);
	check(IsInGameThread());

	SourceToDependency.Empty();
	HandleToListener.Empty();
	AnyDependencyByTag.Empty();
	AnyListenerByHandle.Empty();
	LocalLinkOwners.Empty();
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

	// Execute a copy: the callback may unregister this listener, which destroys the stored delegate mid-call.
	const FDependencyStateEvent Callback = ListenerEntry->Event_StateChanged;
	Callback.ExecuteIfBound(bIsHierarchyDependencyValid, GetSourceObjForBroadcast(SourceObject));

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
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::PruneStaleSources);
	check(IsInGameThread());

	// A collected link owner can no longer unlink itself: drop its local links so they do not stay valid forever.
	TSet<FObjectKey> StaleLinkOwners;
	for (const TPair<FGameplayTag, FObjectKey>& Pair : LocalLinkOwners)
	{
		if (!Pair.Value.ResolveObjectPtr())
		{
			StaleLinkOwners.Add(Pair.Value);
		}
	}

	for (const FObjectKey& StaleOwner : StaleLinkOwners)
	{
		UnlinkLocalTags(StaleOwner, /* bForceRetrigger */ false);
	}

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
		// A callback below may run a nested dispatch on this entry that drops handles and shrinks the array.
		if (!Entry->ListenerHandles.IsValidIndex(Index))
		{
			continue;
		}

		const FListenerHandle ListenerHandle = Entry->ListenerHandles[Index];

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

			// Execute a copy: the callback may unregister this listener, which destroys the stored delegate
			// mid-call. ListenerEntry and ListenerHandle must not be used after this point.
			const FDependencyStateEvent Callback = ListenerEntry->Event_StateChanged;
			Callback.ExecuteIfBound(bIsValidAllDependency, GetSourceObjForBroadcast(SourceObject));
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

	// Iterate a snapshot: callbacks fired by RegisterDependencyContainer may register or unregister any-source
	// listeners, which edits Container->Entries.
	const TArray<TSharedPtr<FAnyDependencyEntry>, TInlineAllocator<8>> Entries(Container->Entries);

	for (const TSharedPtr<FAnyDependencyEntry>& Entry : Entries)
	{
		// Unregistered by a callback earlier in this loop.
		if (!AnyListenerByHandle.Contains(Entry->AnyListenerHandle))
		{
			continue;
		}

		if (Entry->RegisteredSources.Contains(SourceObject))
		{
			continue;
		}

		// Mark the source first: the callback fired by the registration below may validate this tag again for
		// the same source, which must not register a second per-source listener.
		Entry->RegisteredSources.Add(SourceObject);

		// Create missing entries so a multi-tag listener is attached to all of its tags for this source,
		// not only to the one being validated now.
		const FListenerHandle Handle =
			RegisterDependencyContainer(Entry->Request, SourceObject, /* bCreateIfDoesntExist */ true);

		if (!Handle.IsValid())
		{
			Entry->RegisteredSources.Remove(SourceObject);

			if (bForceRetrigger)
			{
				const FDependencyStateEvent Callback = Entry->Request.OnDependencyStateChanged;
				Callback.ExecuteIfBound(false, GetSourceObjForBroadcast(SourceObject));
			}

			continue;
		}

		// The registration callback may have unregistered the any-source listener, which already removed the
		// per-source listeners it knew about; drop the one just created as well.
		if (const TUniquePtr<FAnyDepListenerEntry>* AnyListener = AnyListenerByHandle.Find(Entry->AnyListenerHandle))
		{
			AnyListener->Get()->Handles.Add(Handle);
		}
		else
		{
			HandleToListener.Remove(Handle);
		}
	}
}

UObject* FDependencySignalDispatcher::GetSourceObjForBroadcast(const UObject* SourceObject) const
{
	return SourceObject != WorldContext.Get() ? const_cast<UObject*>(SourceObject) : nullptr;
}

bool FDependencySignalDispatcher::IsLocalSession(const UObject* SourceObject) const
{
	return !IsValid(SourceObject) || SourceObject == WorldContext.Get();
}

void FDependencySignalDispatcher::UnlinkLocalTags(const FObjectKey& OwnerKey, const bool bForceRetrigger)
{
	TArray<FGameplayTag> Tags;
	for (const TPair<FGameplayTag, FObjectKey>& Pair : LocalLinkOwners)
	{
		if (Pair.Value == OwnerKey)
		{
			Tags.Add(Pair.Key);
		}
	}

	DependencySignal::Private::SortParentsFirst(Tags);

	for (const FGameplayTag& Tag : Tags)
	{
		// A listener notified by an earlier tag may have linked this one to another owner: leave it alone then.
		// Validate() removes the tag from LocalLinkOwners.
		if (const FObjectKey* CurrentOwner = LocalLinkOwners.Find(Tag); CurrentOwner && *CurrentOwner == OwnerKey)
		{
			Validate(Tag, /* bIsValid */ false, /* SourceObject */ nullptr, bForceRetrigger);
		}
	}
}

void FDependencySignalDispatcher::InvalidateTags(
	TArray<FGameplayTag>&& Tags, const UObject* SourceObject, const bool bForceRetrigger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDependencySignalDispatcher::InvalidateTags);

	DependencySignal::Private::SortParentsFirst(Tags);

	for (const FGameplayTag& Tag : Tags)
	{
		Validate(Tag, /* bIsValid */ false, SourceObject, bForceRetrigger);
	}
}