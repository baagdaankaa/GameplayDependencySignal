// Copyright Bohdan Sofronii. All Rights Reserved.

#include "GameplayDebuggerCategory_DependencySignal.h"

#if WITH_GAMEPLAY_DEBUGGER

	#include "DependencySignalDispatcher.h"
	#include "GameplayDebuggerTypes.h"
	#include "Components/ActorComponent.h"
	#include "Engine/Canvas.h"
	#include "Engine/World.h"
	#include "GameFramework/Actor.h"
	#include "GameFramework/PlayerController.h"
	#include "Systems/DependencySignalSubsystem.h"

const FName FGameplayDebuggerCategory_DependencySignal::CategoryName(TEXT("DependencySignal"));

namespace DependencySignal::Debug::Private
{
	FString CleanupName(FString Str)
	{
		Str.RemoveFromStart(TEXT("Default__"));
		Str.RemoveFromEnd(TEXT("_C"));
		return Str;
	}

	FString GetNetModeName(const ENetMode NetMode)
	{
		switch (NetMode)
		{
			case NM_Standalone:
				return TEXT("Standalone");
			case NM_DedicatedServer:
				return TEXT("Dedicated Server");
			case NM_ListenServer:
				return TEXT("Listen Server");
			case NM_Client:
				return TEXT("Client");
			default:
				return TEXT("Unknown");
		}
	}

	FString GetSourceName(const UObject* SourceObject, const UObject* WorldContext)
	{
		if (!IsValid(SourceObject))
		{
			return TEXT("<stale / GC'd object>");
		}

		if (SourceObject == WorldContext)
		{
			return TEXT("<Local Session>");
		}

		if (const UActorComponent* Component = Cast<UActorComponent>(SourceObject))
		{
			if (const AActor* Owner = Component->GetOwner())
			{
				return FString::Printf(
					TEXT("%s.%s"), *CleanupName(Owner->GetName()), *CleanupName(Component->GetName()));
			}
		}

		return CleanupName(SourceObject->GetName());
	}

	/** True if the source is the debug actor itself or one of its components. */
	bool IsDebugTarget(const UObject* SourceObject, const AActor* DebugActor)
	{
		if (!DebugActor || !IsValid(SourceObject))
		{
			return false;
		}

		if (SourceObject == DebugActor)
		{
			return true;
		}

		const UActorComponent* Component = Cast<UActorComponent>(SourceObject);
		return Component && Component->GetOwner() == DebugActor;
	}

	/** Removes GameplayDebugger color markup ({green}, {red}, ...) so the visible text can be measured. */
	FString StripMarkup(const FString& Line)
	{
		FString Result;
		Result.Reserve(Line.Len());

		bool bInsideTag = false;
		for (const TCHAR Char : Line)
		{
			if (Char == TEXT('{'))
			{
				bInsideTag = true;
			}
			else if (Char == TEXT('}') && bInsideTag)
			{
				bInsideTag = false;
			}
			else if (!bInsideTag)
			{
				Result.AppendChar(Char);
			}
		}

		return Result;
	}

	/** Lines assigned to columns, before anything is drawn. */
	struct FColumnLayout
	{
		struct FPlacedLine
		{
			const FString* Text = nullptr;
			int32 Column = 0;
			float Y = 0.f;
		};

		TArray<FPlacedLine> Lines;
		TArray<float> ColumnWidths;
	};

	/**
	 * Assigns blocks of lines to columns top to bottom, starting a new column whenever the next line would run
	 * past ColumnBottom. A block (source + its tags) is moved to a fresh column as a whole when it fits there,
	 * so a source header is not separated from its tags.
	 */
	void LayoutColumns(const FGameplayDebuggerCanvasContext& CanvasContext, const TArray<TArray<FString>>& Blocks,
		const float LineHeight, const float ColumnTop, const float ColumnBottom, FColumnLayout& OutLayout)
	{
		float LineY = ColumnTop;
		OutLayout.ColumnWidths.Add(0.f);

		auto StartNewColumn = [&]() {
			OutLayout.ColumnWidths.Add(0.f);
			LineY = ColumnTop;
		};

		for (const TArray<FString>& Block : Blocks)
		{
			const float BlockHeight = Block.Num() * LineHeight;
			if (LineY > ColumnTop && LineY + BlockHeight > ColumnBottom && BlockHeight <= ColumnBottom - ColumnTop)
			{
				StartNewColumn();
			}

			for (const FString& Line : Block)
			{
				if (LineY > ColumnTop && LineY + LineHeight > ColumnBottom)
				{
					StartNewColumn();
				}

				const int32 Column = OutLayout.ColumnWidths.Num() - 1;
				OutLayout.Lines.Add({ &Line, Column, LineY });

				float LineWidth = 0.f;
				float Unused = 0.f;
				CanvasContext.MeasureString(StripMarkup(Line), LineWidth, Unused);
				OutLayout.ColumnWidths[Column] = FMath::Max(OutLayout.ColumnWidths[Column], LineWidth);

				LineY += LineHeight;
			}
		}
	}

	/**
	 * Groups consecutive columns into pages no wider than AvailableWidth and returns the first column of each
	 * page. A page always holds at least one column, even if that column alone is wider than the screen.
	 */
	TArray<int32> PaginateColumns(
		const TArray<float>& ColumnWidths, const float ColumnSpacing, const float AvailableWidth)
	{
		TArray<int32> PageStarts = { 0 };
		float PageWidth = 0.f;

		for (int32 Column = 0; Column < ColumnWidths.Num(); ++Column)
		{
			const float Width = ColumnWidths[Column];
			if (Column == PageStarts.Last())
			{
				PageWidth = Width;
			}
			else if (PageWidth + ColumnSpacing + Width > AvailableWidth)
			{
				PageStarts.Add(Column);
				PageWidth = Width;
			}
			else
			{
				PageWidth += ColumnSpacing + Width;
			}
		}

		return PageStarts;
	}

} // namespace DependencySignal::Debug::Private

FGameplayDebuggerCategory_DependencySignal::FGameplayDebuggerCategory_DependencySignal()
{
	// The dispatcher is worldwide state, so show it even when no debug actor is selected.
	bShowOnlyWithDebugActor = false;
	CollectDataInterval = 0.25f;

	SetDataPackReplication<FRepData>(&DataPack);

	const FGameplayDebuggerInputHandlerConfig ToggleStateConfig(
		TEXT("ToggleServerClientState"), EKeys::V.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(ToggleStateConfig, this, &FGameplayDebuggerCategory_DependencySignal::ToggleDisplayedState);

	const FGameplayDebuggerInputHandlerConfig NextPageConfig(
		TEXT("NextPage"), EKeys::N.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(NextPageConfig, this, &FGameplayDebuggerCategory_DependencySignal::NextPage);

	const FGameplayDebuggerInputHandlerConfig PreviousPageConfig(
		TEXT("PreviousPage"), EKeys::B.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(PreviousPageConfig, this, &FGameplayDebuggerCategory_DependencySignal::PreviousPage);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_DependencySignal::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_DependencySignal());
}

void FGameplayDebuggerCategory_DependencySignal::FRepData::Serialize(FArchive& Ar)
{
	Ar << NetModeName;
	Ar << bHasDispatcher;
	Ar << ListenersNum;
	Ar << AnyListenersNum;
	Ar << Sources;
}

void FGameplayDebuggerCategory_DependencySignal::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	GatherDispatcherState(OwnerPC ? OwnerPC->GetWorld() : nullptr, DebugActor, DataPack);
}

void FGameplayDebuggerCategory_DependencySignal::ToggleDisplayedState()
{
	// In standalone the authority and the local world are the same, so there is nothing to switch between.
	if (!bCanToggleState)
	{
		return;
	}

	// Bound with the default (local) input mode: only the local player's view changes, the authority keeps
	// collecting and replicating its state, so switching back to the server view is instant.
	bShowServerState = !bShowServerState;
	CurrentPage = 0;

	if (bShowServerState)
	{
		LocalData = FRepData();
	}
}

void FGameplayDebuggerCategory_DependencySignal::NextPage()
{
	CurrentPage = (CurrentPage + 1) % FMath::Max(NumPages, 1);
}

void FGameplayDebuggerCategory_DependencySignal::PreviousPage()
{
	const int32 PagesNum = FMath::Max(NumPages, 1);
	CurrentPage = (CurrentPage - 1 + PagesNum) % PagesNum;
}

void FGameplayDebuggerCategory_DependencySignal::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	const UWorld* World = OwnerPC ? OwnerPC->GetWorld() : nullptr;
	const ENetMode NetMode = OwnerPC ? World->GetNetMode() : NM_MAX;

	bCanToggleState = World && NetMode != NM_Standalone && NetMode != NM_ListenServer;
	if (!bCanToggleState && !bShowServerState)
	{
		bShowServerState = true;
		LocalData = FRepData();
	}

	if (!bShowServerState)
	{
		GatherDispatcherState(World, FindLocalDebugActor(), LocalData);
	}

	const FRepData& Data = bShowServerState ? DataPack : LocalData;

	if (bCanToggleState)
	{
		CanvasContext.Printf(TEXT("View: {yellow}%s{white} (%s)  [{yellow}%s{white}]: show %s state"),
			bShowServerState ? TEXT("Server") : TEXT("Client"),
			Data.NetModeName.IsEmpty() ? TEXT("n/a") : *Data.NetModeName, *GetInputHandlerDescription(0),
			bShowServerState ? TEXT("client") : TEXT("server"));
	}
	else
	{
		using namespace DependencySignal::Debug::Private;

		CanvasContext.Printf(TEXT("View: {yellow}%s"), *GetNetModeName(NetMode));
	}

	DrawState(Data, CanvasContext);
}

void FGameplayDebuggerCategory_DependencySignal::GatherDispatcherState(
	const UWorld* World, const AActor* DebugActor, FRepData& OutData)
{
	using namespace DependencySignal::Debug::Private;

	OutData = FRepData();

	if (!World)
	{
		return;
	}

	OutData.NetModeName = GetNetModeName(World->GetNetMode());

	const UDependencySignalSubsystem* Subsystem = World->GetSubsystem<UDependencySignalSubsystem>();
	if (!Subsystem || !Subsystem->Dispatcher.IsValid())
	{
		return;
	}

	const FDependencySignalDispatcher& Dispatcher = *Subsystem->Dispatcher;
	const UObject* WorldContext = Dispatcher.WorldContext.Get();

	OutData.bHasDispatcher = true;
	OutData.ListenersNum = Dispatcher.GetListenersNum();
	OutData.AnyListenersNum = Dispatcher.GetAnyListenersNum();
	OutData.Sources.Reserve(Dispatcher.SourceToDependency.Num());

	for (const auto& SourcePair : Dispatcher.SourceToDependency)
	{
		if (!SourcePair.Value.IsValid())
		{
			continue;
		}

		const UObject* SourceObject = SourcePair.Key.ResolveObjectPtr();

		FRepData::FSourceEntry& Source = OutData.Sources.AddDefaulted_GetRef();
		Source.SourceName = GetSourceName(SourceObject, WorldContext);
		Source.bIsDebugTarget = IsDebugTarget(SourceObject, DebugActor);
		Source.bIsLocalSession = SourceObject && SourceObject == WorldContext;
		Source.Tags.Reserve(SourcePair.Value->TagToEntry.Num());

		for (const auto& TagPair : SourcePair.Value->TagToEntry)
		{
			FRepData::FTagEntry& Tag = Source.Tags.AddDefaulted_GetRef();
			Tag.TagName = TagPair.Key.ToString();

			if (const auto* DepEntry = TagPair.Value.Get())
			{
				Tag.ListenersNum = DepEntry->ListenerHandles.Num();
				Tag.bIsValid = DepEntry->State.bIsValid != 0;
				Tag.bIsValidParent = DepEntry->State.bIsValidParent != 0;
			}
		}

		Source.Tags.Sort([](const FRepData::FTagEntry& A, const FRepData::FTagEntry& B) {
			return A.TagName < B.TagName;
		});
	}

	// The local session container only exists once something registered or validated without a source object,
	// which typically never happens on a server. Always list it so both views have the same layout.
	if (WorldContext && !OutData.Sources.ContainsByPredicate([](const FRepData::FSourceEntry& Source) {
			return Source.bIsLocalSession;
		}))
	{
		FRepData::FSourceEntry& Source = OutData.Sources.AddDefaulted_GetRef();
		Source.SourceName = GetSourceName(WorldContext, WorldContext);
		Source.bIsLocalSession = true;
	}

	// Debug target sources first, then the local session, then alphabetically.
	OutData.Sources.Sort([](const FRepData::FSourceEntry& A, const FRepData::FSourceEntry& B) {
		if (A.bIsDebugTarget != B.bIsDebugTarget)
		{
			return A.bIsDebugTarget;
		}
		if (A.bIsLocalSession != B.bIsLocalSession)
		{
			return A.bIsLocalSession;
		}
		return A.SourceName < B.SourceName;
	});
}

void FGameplayDebuggerCategory_DependencySignal::DrawState(
	const FRepData& Data, FGameplayDebuggerCanvasContext& CanvasContext)
{
	using namespace DependencySignal::Debug::Private;

	NumPages = 1;

	if (Data.NetModeName.IsEmpty())
	{
		CanvasContext.Printf(TEXT("{grey}Waiting for data..."));
		return;
	}

	if (!Data.bHasDispatcher)
	{
		CanvasContext.Printf(TEXT("{red}No DependencySignal dispatcher in this world."));
		return;
	}

	const FString Summary = FString::Printf(
		TEXT("Sources: {yellow}%d{white}  Listeners: {yellow}%d{white}  Any-source listeners: {yellow}%d"),
		Data.Sources.Num(), Data.ListenersNum, Data.AnyListenersNum);

	if (Data.Sources.IsEmpty())
	{
		CanvasContext.Print(Summary);
		CanvasContext.Printf(TEXT("{grey}No source objects registered."));
		return;
	}

	TArray<TArray<FString>> Blocks;
	Blocks.Reserve(Data.Sources.Num());

	for (const FRepData::FSourceEntry& Source : Data.Sources)
	{
		TArray<FString>& Lines = Blocks.AddDefaulted_GetRef();
		Lines.Reserve(Source.Tags.Num() + 1);

		Lines.Add(FString::Printf(
			TEXT("%s[Source] %s"), Source.bIsDebugTarget ? TEXT("{cyan}> ") : TEXT("{yellow}"), *Source.SourceName));

		if (Source.Tags.IsEmpty())
		{
			Lines.Add(TEXT("{grey}    (no tags)"));
			continue;
		}

		for (const FRepData::FTagEntry& Tag : Source.Tags)
		{
			const bool bIsHierarchyValid = Tag.bIsValid && Tag.bIsValidParent;

			// Orange: the tag itself is valid, but an ancestor in its hierarchy is not.
			const TCHAR* Color = bIsHierarchyValid ? TEXT("{green}") : Tag.bIsValid ? TEXT("{orange}") : TEXT("{red}");
			const TCHAR* Note = (!bIsHierarchyValid && Tag.bIsValid) ? TEXT(" (parent invalid)") : TEXT("");

			Lines.Add(
				FString::Printf(TEXT("    %s%s{grey}%s  listeners: %d"), Color, *Tag.TagName, Note, Tag.ListenersNum));
		}
	}

	const UCanvas* Canvas = CanvasContext.Canvas.Get();

	float Unused = 0.f;
	float LineHeight = 0.f;
	CanvasContext.MeasureString(TEXT("Wg"), Unused, LineHeight);

	if (!Canvas || LineHeight <= 0.f)
	{
		CanvasContext.Print(Summary);
		for (const TArray<FString>& Block : Blocks)
		{
			for (const FString& Line : Block)
			{
				CanvasContext.Print(Line);
			}
		}
		return;
	}

	float ColumnSpacing = 0.f;
	CanvasContext.MeasureString(TEXT("        "), ColumnSpacing, Unused);

	// The summary line is printed after the layout because it shows the page count; reserve its row.
	const float StartX = CanvasContext.CursorX;
	const float ColumnTop = CanvasContext.CursorY + LineHeight;
	const float ColumnBottom = FMath::Max(ColumnTop + LineHeight, Canvas->ClipY - LineHeight);

	FColumnLayout Layout;
	LayoutColumns(CanvasContext, Blocks, LineHeight, ColumnTop, ColumnBottom, Layout);

	const TArray<int32> PageStarts =
		PaginateColumns(Layout.ColumnWidths, ColumnSpacing, Canvas->ClipX - StartX - ColumnSpacing);
	NumPages = PageStarts.Num();
	CurrentPage = FMath::Clamp(CurrentPage, 0, NumPages - 1);

	if (NumPages > 1)
	{
		CanvasContext.Printf(
			TEXT("%s  Page: {yellow}%d/%d{white}  [{yellow}%s{white}]: next  [{yellow}%s{white}]: back"), *Summary,
			CurrentPage + 1, NumPages, *GetInputHandlerDescription(1), *GetInputHandlerDescription(2));
	}
	else
	{
		CanvasContext.Print(Summary);
	}

	// X position of every column on the current page (columns on other pages are skipped below).
	const int32 FirstColumn = PageStarts[CurrentPage];
	const int32 EndColumn = CurrentPage + 1 < NumPages ? PageStarts[CurrentPage + 1] : Layout.ColumnWidths.Num();

	TArray<float> ColumnX;
	ColumnX.Init(0.f, Layout.ColumnWidths.Num());

	float NextColumnX = StartX;
	for (int32 Column = FirstColumn; Column < EndColumn; ++Column)
	{
		ColumnX[Column] = NextColumnX;
		NextColumnX += Layout.ColumnWidths[Column] + ColumnSpacing;
	}

	float LowestY = ColumnTop;
	for (const FColumnLayout::FPlacedLine& Line : Layout.Lines)
	{
		if (Line.Column < FirstColumn || Line.Column >= EndColumn)
		{
			continue;
		}

		CanvasContext.CursorX = ColumnX[Line.Column];
		CanvasContext.CursorY = Line.Y;
		CanvasContext.Print(*Line.Text);

		LowestY = FMath::Max(LowestY, Line.Y + LineHeight);
	}

	CanvasContext.CursorX = CanvasContext.DefaultX;
	CanvasContext.CursorY = LowestY;
}

#endif // WITH_GAMEPLAY_DEBUGGER
