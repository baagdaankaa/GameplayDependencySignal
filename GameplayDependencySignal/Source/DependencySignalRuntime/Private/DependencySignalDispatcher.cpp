// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalDispatcher.h"

#include "DependencySignalRuntime.h"
#include "DependencySignalTags.h"
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

	return RegisterDependencyContainer(MoveTemp(Request), /* bCreateIfDoesntExist */ true);
}

void FDependencySignalDispatcher::Unregister(FListenerHandle& ListenerHandle)
{
	if (HandleToListener.Remove(ListenerHandle) == 0)
	{
		return;
	}

	ListenerHandle.Reset();
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

void FDependencySignalDispatcher::Validate(
	const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FGameplayDependencyResolver::Validate [%s]"), *Tag.ToString()));

	if (!IsValidDependencyTag(Tag))
	{
		DS_UE_LOG(Warning, TEXT("The operation failed due to invalid tag [%s]"), *Tag.ToString());
		return;
	}

	// the `const` on the method is a lie
	FDependencySignalDispatcher& MutableThis = const_cast<FDependencySignalDispatcher&>(*this);

	FDepEntry* Entry = MutableThis.GetDependencyEntry(SourceObject, Tag, /* bCreateIfDoesntExist */ true);
	check(Entry != nullptr);

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
	return ensureAlwaysMsgf(Tag.MatchesTag(DependencySignalTags::Root), TEXT("Tag must be inherited from [%s] tag"),
		*DependencySignalTags::Root.GetTag().ToString());
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::GenerateNewListenerHandle()
{
	static uint64 HandleId = 0;
	return FListenerHandle(++HandleId, AsShared());
}

FDependencySignalDispatcher::FListenerHandle FDependencySignalDispatcher::RegisterDependencyContainer(
	FDependencySignalRequest&& Request, const bool bCreateIfDoesntExist)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(
		TEXT("FGameplayDependencyResolver::RegisterDependencyContainer [%s]"), *Request.DependencyTags.ToString()));

	const FListenerHandle& NewHandle = GenerateNewListenerHandle();
	check(NewHandle.IsValid());

	TUniquePtr<FDepListenerEntry> NewListenerEntry = MakeUnique<FDepListenerEntry>();
	NewListenerEntry->DependencyTags = Request.DependencyTags;
	NewListenerEntry->Event_StateChanged = MoveTemp(Request.OnDependencyStateChanged);

	HandleToListener.Add(NewHandle, MoveTemp(NewListenerEntry));

	bool bIsHierarchyDependencyValid = false;
	for (const FGameplayTag& DependencyTag : Request.DependencyTags)
	{
		FDepEntry* DependencyEntry =
			GetDependencyEntry(Request.SourceObject.Get(), DependencyTag, bCreateIfDoesntExist);
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
		bIsHierarchyDependencyValid, GetSourceObjForBroadcast(Request.SourceObject.Get()));

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

FDependencySignalDispatcher::FDepEntry* FDependencySignalDispatcher::GetDependencyEntry(
	const UObject* SourceObject, const FGameplayTag& Tag, bool bCreateIfDoesntExist)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FGameplayDependencyResolver::GetDependencyEntry [%s]"), *Tag.ToString()));

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
		*FString::Printf(TEXT("FGameplayDependencyResolver::CreateDependencyHierarchy [%s]"), *Tag.ToString()));

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

void FDependencySignalDispatcher::ShareDependencyState(
	FDepEntry* RootEntry, const UObject* SourceObject, const bool bForceRetrigger)
{
	check(RootEntry != nullptr);

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(
		*FString::Printf(TEXT("FGameplayDependencyResolver::ShareDependencyState [%s]"), *RootEntry->Tag.ToString()));

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
		TEXT("FGameplayDependencyResolver::HandleDependencyStateChanges [%s]"), *Entry->Tag.ToString()));

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

UObject* FDependencySignalDispatcher::GetSourceObjForBroadcast(const UObject* SourceObject) const
{
	return SourceObject != WorldContext.Get() ? const_cast<UObject*>(SourceObject) : nullptr;
}