// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalDispatcher.h"

#include "DependencySignalRuntime.h"
#include "Algo/AllOf.h"

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
		for (const FListenerHandle& Handle : Listener->Get()->Handles)
		{
			AnyListenerByHandle.Remove(Handle);
		}

		if (Listener->Get()->Entry.IsValid())
		{
			const TSharedPtr<FAnyDependencyEntry> Entry = Listener->Get()->Entry.Pin();

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
	return Algo::AllOf(Tags, [&](const FGameplayTag& Tag) {
		return IsValidDependency(Tag, SourceObject);
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
	const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::Validate [%s]"), *Tag.ToString()));

	// the `const` on the method is a lie
	FDependencySignalDispatcher& MutableThis = const_cast<FDependencySignalDispatcher&>(*this);

	FDepEntry* Entry = MutableThis.GetDependencyEntry(SourceObject, Tag, /* bCreateIfDoesntExist */ true);
	check(Entry != nullptr);

	MutableThis.ValidateAnyDependency(Tag, SourceObject, bForceRetrigger);

	if (bForceRetrigger || Entry->State.bIsValid != bIsValid)
	{
		Entry->State.bIsValid = bIsValid;

		if (bForceRetrigger || Entry->State.bIsValidParent)
		{
			MutableThis.HandleDependencyStateChanges(Entry, SourceObject, bForceRetrigger);
		}

		MutableThis.ShareDependencyState(Entry, SourceObject, bForceRetrigger);
	}
}

void FDependencySignalDispatcher::Reset()
{
	SourceToDependency.Empty();
	HandleToListener.Empty();

	RootTags.Reset();
	WorldContext.Reset();
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
	static uint64 HandleId = 0;
	return FListenerHandle(++HandleId, AsShared());
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::RegisterDependencyContainer(
	const FDependencySignalRequest& Request, const UObject* SourceObject, const bool bCreateIfDoesntExist)
{
	if (!IsValidDependencyTags(Request.DependencyTags))
	{
		DS_UE_LOG(Warning, TEXT("The operation failed due to invalid tags [%s]"), *Request.DependencyTags.ToString());
		return FListenerHandle();
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(
		TEXT("FDependencySignalDispatcher::RegisterDependencyContainer [%s]"), *Request.DependencyTags.ToString()));

	const FListenerHandle& NewHandle = GenerateNewListenerHandle();
	check(NewHandle.IsValid());

	TUniquePtr<FDepListenerEntry> NewListenerEntry = MakeUnique<FDepListenerEntry>();
	NewListenerEntry->DependencyTags = Request.DependencyTags;
	NewListenerEntry->Event_StateChanged = Request.OnDependencyStateChanged;

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

	TUniquePtr<FAnyDepListenerEntry> NewListenerEntry = MakeUnique<FAnyDepListenerEntry>();
	NewListenerEntry->Handles.Reserve(SourceToDependency.Num());
	NewListenerEntry->Entry = NewEntry;

	for (const TPair<FObjectKey, TUniquePtr<FDepContainer>>& SourceDependency : SourceToDependency)
	{
		const UObject* SourceObject = SourceDependency.Key.ResolveObjectPtr();
		if (!IsValid(SourceObject))
		{
			continue;
		}

		const FListenerHandle& Handle = RegisterDependencyContainer(Request, SourceObject);
		if (!Handle.IsValid())
		{
			continue;
		}

		NewEntry->RegisteredSources.Add(SourceDependency.Key);
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
	// if a SourceObject was removed by the GC, we must remove its dependency
	SourceToDependency.Remove(nullptr);

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
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::GetDependencyEntry [%s]"), *Tag.ToString()));

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
	return const_cast<FDependencySignalDispatcher*>(this)->GetDependencyEntry(SourceObject, Tag);
}

void FDependencySignalDispatcher::InitDependencyEntry(FDepEntry& NewEntry) const
{
	const bool bIsRootTag = RootTags.HasTagExact(NewEntry.Tag);

	NewEntry.State.bIsValid = bIsRootTag;
	NewEntry.State.bIsValidParent = bIsRootTag;
}

void FDependencySignalDispatcher::CreateDependencyHierarchy(FDepContainer& Container, const FGameplayTag& Tag) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::CreateDependencyHierarchy [%s]"), *Tag.ToString()));

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
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::CreateAnyDependencyContainer [%s]"), *Tag.ToString()));

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

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::ShareDependencyState [%s]"), *RootEntry->Tag.ToString()));

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
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(
		TEXT("FDependencySignalDispatcher::HandleDependencyStateChanges [%s]"), *Entry->Tag.ToString()));

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
			Entry->ListenerHandles.RemoveAt(Index);
			HandleToListener.Remove(ListenerHandle);
			continue;
		}

		const bool bIsValidAllDependency = IsValidDependency(ListenerEntry->DependencyTags, SourceObject);

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
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FDependencySignalDispatcher::ValidateAnyDependency [%s]"), *Tag.ToString()));

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

		const FListenerHandle& Handle = RegisterDependencyContainer(Entry->Request, SourceObject);

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