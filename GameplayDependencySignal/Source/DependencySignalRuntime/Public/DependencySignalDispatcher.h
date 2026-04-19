// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalTypes.h"
#include "GameplayTagContainer.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Central dispatcher for managing dependency signal listeners and validations.
 *
 * The dispatcher is responsible for:
 * - Registering and unregistering dependency signal listeners
 * - Maintaining the hierarchy and validity state of dependencies
 * - Broadcasting validity changes to all interested listeners
 *
 * This class uses a hierarchical approach where dependencies can have parent dependencies,
 * and validity is determined by both the local state and the parent state.
 */
class UE_API FDependencySignalDispatcher : public TSharedFromThis<FDependencySignalDispatcher>
{
public:
	UE_NONCOPYABLE(FDependencySignalDispatcher);

	/**
	 * Constructs the dispatcher with root dependency tags.
	 *
	 * @param InRootTags		The root tags that define the base of the dependency hierarchy.
	 * @param InWorldContext	World context used for a fallback SourceObject for local session dependency resolution
	 */
	FDependencySignalDispatcher(const FGameplayTagContainer& InRootTags, const UObject* InWorldContext);

	/**
	 * Destructor for the dispatcher.
	 *
	 * Cleans up all registered listeners and dependency entries.
	 */
	virtual ~FDependencySignalDispatcher();

	/**
	 * Registers a dependency signal listener with the dispatcher.
	 *
	 * The listener will be invoked whenever the validity state of the dependency tags
	 * it's listening to changes. Returns a handle that can be used to unregister later.
	 *
	 * @param Request				A rvalue reference to the request containing tags, source, and callback.
	 * @return						A handle identifying the registered listener.
	 */
	FDependencySignalListenerHandle Register(FDependencySignalRequest&& Request);

	/**
	 * Unregisters a previously registered listener using its handle.
	 *
	 * @param ListenerHandle		The handle returned by Register() (passed by reference).
	 */
	void Unregister(FDependencySignalListenerHandle& ListenerHandle);

	/**
	 * Checks whether a single dependency identified by a tag is currently valid.
	 *
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param SourceObject			Optional source object to check validity for a specific source.
	 * @return						True if the dependency is valid, false otherwise.
	 */
	bool IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject = nullptr) const;

	/**
	 * Checks whether all dependencies identified by tags in a container are valid.
	 *
	 * @param Tags					The container of gameplay tags to check.
	 * @param SourceObject			Optional source object to check validity for a specific source.
	 * @return						True if all dependencies are valid, false otherwise.
	 */
	bool IsValidDependency(const FGameplayTagContainer& Tags, const UObject* SourceObject = nullptr) const;

	/**
	 * Updates the validity state of a dependency and broadcasts changes to listeners.
	 *
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param bIsValid				Whether the dependency is now valid or invalid.
	 * @param SourceObject			Optional source object defining the validation scope.
	 * @param bForceRetrigger		If true, notifies listeners even if the state didn't change.
	 */
	void Validate(const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject = nullptr,
		const bool bForceRetrigger = false) const;

	/**
	 * Resets all dependency states and listener registrations.
	 *
	 * Clears all cached validity states and listener handles.
	 * After reset, all dependencies are considered invalid until re-validated.
	 */
	void Reset();

protected:
	/** Determines whether a set of dependency tags is valid. */
	virtual bool IsValidDependencyTags(const FGameplayTagContainer& Tags) const;

	/** Determines whether a single dependency tag is valid. */
	virtual bool IsValidDependencyTag(const FGameplayTag& Tag) const;

private:
	// ---------------------------------------------------------------------------
	// Internal data structures
	// ---------------------------------------------------------------------------

	/** Alias for the listener handle type used internally. */
	using FListenerHandle = FDependencySignalListenerHandle;

	/** Stores the validity state of a dependency entry. */
	struct FEntryState
	{
		FEntryState()
			: bIsValid(0)
			, bIsValidParent(0)
		{
		}

		/** Checks whether the entire hierarchy up to this entry is valid. */
		bool IsHierarchyValid() const
		{
			return bIsValid && bIsValidParent;
		}

		/** Whether this dependency entry itself is valid. */
		uint8 bIsValid : 1;

		/** Whether the parent dependency in the hierarchy is valid. */
		uint8 bIsValidParent : 1;
	};

	/** A single entry in the dependency tree representing one tag and its listeners. */
	struct FDepEntry
	{
		UE_NONCOPYABLE(FDepEntry);

		explicit FDepEntry(const FGameplayTag& InTag)
			: Tag(InTag)
		{
		}

		/** Checks whether the entire hierarchy from root to this entry is valid. */
		bool IsHierarchyValid() const
		{
			return State.IsHierarchyValid();
		}

		/** The gameplay tag this entry represents. */
		const FGameplayTag Tag;

		/** The current validity state of this entry. */
		FEntryState State;

		/** Container of direct child tags under this entry. */
		FGameplayTagContainer ChildTags;

		/** List of listener handles registered for this entry. */
		TArray<FListenerHandle> ListenerHandles;
	};

	/** Container for managing a set of dependency entries for a specific source object. */
	struct FDepContainer
	{
		UE_NONCOPYABLE(FDepContainer);

		FDepContainer() = default;
		explicit FDepContainer(const TFunction<void(FDepEntry&)>& InitEntry)
			: InitEntry(InitEntry)
		{
		}

		/** Finds an existing dependency entry by tag. */
		FDepEntry* FindEntry(const FGameplayTag& Tag)
		{
			if (const TUniquePtr<FDepEntry>* EntryPtr = TagToEntry.Find(Tag))
			{
				return EntryPtr->Get();
			}

			return nullptr;
		}

		/** Creates a dependency entry for the given tag. */
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

		/** Checks whether a tag already has an entry. */
		bool Contains(const FGameplayTag& Tag) const
		{
			return TagToEntry.Contains(Tag);
		}

	private:
		/** Map from gameplay tags to their corresponding entries. */
		TMap<FGameplayTag, TUniquePtr<FDepEntry>> TagToEntry;

		/** Optional function called when initializing new entries. */
		TFunction<void(FDepEntry&)> InitEntry;
	};

	/** Data structure for a registered listener. */
	struct FDepListenerEntry
	{
		UE_NONCOPYABLE(FDepListenerEntry);

		FDepListenerEntry() = default;

		/** Whether the dependency was valid the last time the listener was notified. */
		uint8 bWasValid : 1;

		/** Container of dependency tags this listener is monitoring. */
		FGameplayTagContainer DependencyTags;

		/** The delegate event to invoke when validity changes. */
		FDependencyStateEvent Event_StateChanged;
	};

	// ---------------------------------------------------------------------------
	// Internal methods
	// ---------------------------------------------------------------------------

	/** Generates a new unique listener handle. */
	FListenerHandle GenerateNewListenerHandle();

	/** Registers a dependency signal request and associates it with dependency entries. */
	FListenerHandle RegisterDependencyContainer(
		FDependencySignalRequest&& Request, const bool bCreateIfDoesntExist = false);

	/** Retrieves the dependency container for a specific source object, creating if needed. */
	FDepContainer& GetDependencyContainer(const UObject* SourceObject);

	/** Finds or creates a dependency entry for a tag within a source object's container. */
	FDepEntry* GetDependencyEntry(
		const UObject* SourceObject, const FGameplayTag& Tag, bool bCreateIfDoesntExist = false);

	/** Finds a dependency entry for a tag within a source object's container (const version). */
	const FDepEntry* GetDependencyEntry(const UObject* SourceObject, const FGameplayTag& Tag) const;

	/** Initializes a newly created dependency entry with the default state. */
	void InitDependencyEntry(FDepEntry& NewEntry) const;

	/** Creates the full hierarchy of dependency entries for a tag and its parents. */
	void CreateDependencyHierarchy(FDepContainer& Container, const FGameplayTag& Tag) const;

	/** Shares the validity state of a root entry to all its child listeners. */
	void ShareDependencyState(FDepEntry* RootEntry, const UObject* SourceObject, const bool bForceRetrigger);

	/** Handles state changes for a single entry and shares to children. */
	void HandleDependencyStateChanges(FDepEntry* Entry, const UObject* SourceObject, const bool bForceRetrigger);

	/** Determines the appropriate source object for broadcasting validity changes. */
	UObject* GetSourceObjForBroadcast(const UObject* SourceObject) const;

	// ---------------------------------------------------------------------------
	// Storage
	// ---------------------------------------------------------------------------

	/** Map from source objects to their dependency containers. */
	TMap<FObjectKey, TUniquePtr<FDepContainer>> SourceToDependency;

	/** Map from listener handles to their listener entry data. */
	TMap<FListenerHandle, TUniquePtr<FDepListenerEntry>> HandleToListener;

	/**
	 * The root tags that define the base of the dependency hierarchy.
	 * By default, root tags have a validity state of true.
	 */
	FGameplayTagContainer RootTags;

	/**
	 * Weak ptr to the world context used as a fallback SourceObject
	 * for local session dependency resolution when none is explicitly provided.
	 */
	FWeakObjectPtr WorldContext;
};

#undef UE_API