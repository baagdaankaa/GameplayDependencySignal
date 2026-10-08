// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalRuntime.h"

#if WITH_GAMEPLAY_DEBUGGER
	#include "GameplayDebugger.h"
	#include "GameplayDebuggerCategory_DependencySignal.h"
#endif // WITH_GAMEPLAY_DEBUGGER

DEFINE_LOG_CATEGORY(LogDependencySignal)

#define LOCTEXT_NAMESPACE "FDependencySignalRuntimeModule"

void FDependencySignalRuntimeModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory(FGameplayDebuggerCategory_DependencySignal::CategoryName,
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_DependencySignal::MakeInstance));
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER
}

void FDependencySignalRuntimeModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory(FGameplayDebuggerCategory_DependencySignal::CategoryName);
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_GAMEPLAY_DEBUGGER
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDependencySignalRuntimeModule, DependencySignalRuntime)
