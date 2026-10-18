// Copyright Bohdan Sofronii. All Rights Reserved.

#include "K2Node_ListenForDependencySignal.h"

#include "Async/AsyncAction_ListenForDependencySignal.h"

void UK2Node_ListenForDependencySignal::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	RegisterMenuActions<UAsyncAction_ListenForDependencySignal>(ActionRegistrar);
}
