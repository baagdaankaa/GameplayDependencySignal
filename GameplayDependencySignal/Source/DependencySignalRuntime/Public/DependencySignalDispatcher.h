// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalTypes.h"
#include "GameplayTagContainer.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class UE_API FDependencySignalDispatcher : public TSharedFromThis<FDependencySignalDispatcher>
{
public:
	UE_NONCOPYABLE(FDependencySignalDispatcher);

	FDependencySignalDispatcher(const FGameplayTagContainer& InRootTags, const UObject* InWorldContext = nullptr);
	virtual ~FDependencySignalDispatcher();

	FDependencySignalListenerHandle Register(FDependencySignalRequest&& Request);

	void Unregister(FDependencySignalListenerHandle& ListenerHandle);

	bool IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject = nullptr) const;
	bool IsValidDependency(const FGameplayTagContainer& Tags, const UObject* SourceObject = nullptr) const;

	int32 GetListenersNum() const;

	void Validate(const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject = nullptr,
		const bool bForceRetrigger = false) const;

	void Reset();

protected:
	virtual bool IsValidDependencyTags(const FGameplayTagContainer& Tags) const;
	virtual bool IsValidDependencyTag(const FGameplayTag& Tag) const;

private:
	// ---------------------------------------------------------------------------
	// Internal data structures
	// ---------------------------------------------------------------------------

	using FListenerHandle = FDependencySignalListenerHandle;

	struct FEntryState
	{
		FEntryState()
			: bIsValid(0)
			, bIsValidParent(0)
		{
		}

		bool IsHierarchyValid() const
		{
			return bIsValid && bIsValidParent;
		}

		uint8 bIsValid : 1;
		uint8 bIsValidParent : 1;
	};

	struct FDepEntry
	{
		UE_NONCOPYABLE(FDepEntry);

		explicit FDepEntry(const FGameplayTag& InTag)
			: Tag(InTag)
		{
		}

		bool IsHierarchyValid() const
		{
			return State.IsHierarchyValid();
		}

		const FGameplayTag Tag;

		FEntryState State;
		FGameplayTagContainer ChildTags;
		TArray<FListenerHandle> ListenerHandles;
	};

	struct FDepContainer
	{
		UE_NONCOPYABLE(FDepContainer);

		FDepContainer() = default;
		explicit FDepContainer(const TFunction<void(FDepEntry&)>& InitEntry)
			: InitEntry(InitEntry)
		{
		}

		FDepEntry* FindEntry(const FGameplayTag& Tag)
		{
			if (const TUniquePtr<FDepEntry>* EntryPtr = TagToEntry.Find(Tag))
			{
				return EntryPtr->Get();
			}

			return nullptr;
		}

		FDepEntry* AddEntry(const FGameplayTag& Tag)
		{
			TUniquePtr<FDepEntry> NewEntry = MakeUnique<FDepEntry>(Tag);

			if (InitEntry.IsSet())
			{
				InitEntry(*NewEntry.Get());
			}

			TagToEntry.Add(Tag, MoveTemp(NewEntry));
			return TagToEntry[Tag].Get();
		}

		bool Contains(const FGameplayTag& Tag) const
		{
			return TagToEntry.Contains(Tag);
		}

	private:
		TMap<FGameplayTag, TUniquePtr<FDepEntry>> TagToEntry;

		TFunction<void(FDepEntry&)> InitEntry;
	};

	struct FDepListenerEntry
	{
		UE_NONCOPYABLE(FDepListenerEntry);

		FDepListenerEntry() = default;

		uint8 bWasValid : 1;
		FGameplayTagContainer DependencyTags;
		FDependencyStateEvent Event_StateChanged;
	};

	// ---------------------------------------------------------------------------
	// Internal methods
	// ---------------------------------------------------------------------------

	FListenerHandle GenerateNewListenerHandle();

	FListenerHandle RegisterDependencyContainer(
		FDependencySignalRequest&& Request, const bool bCreateIfDoesntExist = false);

	FDepContainer& GetDependencyContainer(const UObject* SourceObject);
	FDepEntry* GetDependencyEntry(
		const UObject* SourceObject, const FGameplayTag& Tag, bool bCreateIfDoesntExist = false);
	const FDepEntry* GetDependencyEntry(const UObject* SourceObject, const FGameplayTag& Tag) const;

	void InitDependencyEntry(FDepEntry& NewEntry) const;
	void CreateDependencyHierarchy(FDepContainer& Container, const FGameplayTag& Tag) const;

	void ShareDependencyState(FDepEntry* RootEntry, const UObject* SourceObject, const bool bForceRetrigger);
	void HandleDependencyStateChanges(FDepEntry* Entry, const UObject* SourceObject, const bool bForceRetrigger);

	UObject* GetSourceObjForBroadcast(const UObject* SourceObject) const;

	// ---------------------------------------------------------------------------
	// Storage
	// ---------------------------------------------------------------------------

	TMap<FObjectKey, TUniquePtr<FDepContainer>> SourceToDependency;
	TMap<FListenerHandle, TUniquePtr<FDepListenerEntry>> HandleToListener;

	FGameplayTagContainer RootTags;
	FWeakObjectPtr WorldContext;
};

#undef UE_API