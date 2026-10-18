// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_DependencySignalAsyncAction.h"
#include "K2Node_ListenForDependencySignal.generated.h"

/**
 * Blueprint node for UAsyncAction_ListenForDependencySignal::ListenForDependencySignal.
 *
 * Works like the generic async action node, but hides the Source Object pin while Any Source is checked.
 */
UCLASS()
class UK2Node_ListenForDependencySignal : public UK2Node_DependencySignalAsyncAction
{
	GENERATED_BODY()

public:
	//~ Begin UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface
};
