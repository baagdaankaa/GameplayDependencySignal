// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

	#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;
class UWorld;

/**
 * GameplayDebugger category that shows the state of FDependencySignalDispatcher.
 *
 * Two views, switched with the toggle key (Shift+V by default, shown in the category header):
 *   - Server: CollectData() runs on the authority (the server in a networked game), the GameplayDebugger
 *             replicates the data pack, and DrawData() draws it on the local player - so a client sees the
 *             server's dispatcher, not its own.
 *   - Client: DrawData() reads the dispatcher of the local player's own world directly.
 * In standalone the local world is the authority, so switching is disabled, and the local dispatcher is shown.
 *
 * When the list does not fit vertically on screen, it continues in additional columns. When the columns do not
 * fit horizontally, they are split into pages, switched with Shift+N (next) / Shift+B (back) by default.
 *
 * Usage in-game:
 *   Press its number key to enable, and it can be toggled. Sources owned by the selected debug actor are listed first.
 */
class FGameplayDebuggerCategory_DependencySignal : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_DependencySignal();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	//~ Begin FGameplayDebuggerCategory Interface
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
	//~ End FGameplayDebuggerCategory Interface

	/** The name the category is registered under in the GameplayDebugger. */
	static const FName CategoryName;

private:
	/** Dispatcher state flattened for drawing; replicated from the authority for the server view. */
	struct FRepData
	{
		/** One dependency tag registered for a source. */
		struct FTagEntry
		{
			FString TagName;
			int32 ListenersNum = 0;
			bool bIsValid = false;
			bool bIsValidParent = false;

			/** Source that owns the tag's local link; only set for local session tags. */
			FString LinkOwnerName;

			friend FArchive& operator<<(FArchive& Ar, FTagEntry& Entry)
			{
				return Ar << Entry.TagName << Entry.ListenersNum << Entry.bIsValid << Entry.bIsValidParent
						  << Entry.LinkOwnerName;
			}
		};

		/** One registered source object and its tags. */
		struct FSourceEntry
		{
			FString SourceName;
			bool bIsDebugTarget = false;

			/** Only used to order sources while gathering; the order itself is what gets replicated. */
			bool bIsLocalSession = false;
			TArray<FTagEntry> Tags;

			friend FArchive& operator<<(FArchive& Ar, FSourceEntry& Entry)
			{
				return Ar << Entry.SourceName << Entry.bIsDebugTarget << Entry.Tags;
			}
		};

		void Serialize(FArchive& Ar);

		FString NetModeName;
		bool bHasDispatcher = false;
		int32 ListenersNum = 0;
		int32 AnyListenersNum = 0;
		TArray<FSourceEntry> Sources;
	};

	/** Fills OutData from the dispatcher of the given world. */
	static void GatherDispatcherState(const UWorld* World, const AActor* DebugActor, FRepData& OutData);

	/**
	 * Draws the given state below the category header, wrapping into columns when it runs off the bottom of the
	 * screen and splitting the columns into pages when they run off the right side.
	 */
	void DrawState(const FRepData& Data, FGameplayDebuggerCanvasContext& CanvasContext);

	/** Input handler: switches between the server (replicated) and client (local) view. */
	void ToggleDisplayedState();

	/** Input handler: shows the next page, wrapping to the first. */
	void NextPage();

	/** Input handler: shows the previous page, wrapping to the last. */
	void PreviousPage();

	/** State replicated from the authority. */
	FRepData DataPack;

	/** State of the local player's own world, gathered while drawing the client view. */
	FRepData LocalData;

	/** Page currently drawn; clamped against NumPages on every draw. */
	int32 CurrentPage = 0;

	/** Number of pages computed by the last draw. */
	int32 NumPages = 1;

	/** If true, draw the replicated server's dispatcher instead of the local client state. */
	bool bShowServerState = false;

	/** False in standalone, where the server and client views would be identical; refreshed on every draw. */
	bool bCanToggleState = false;
};

#endif // WITH_GAMEPLAY_DEBUGGER
