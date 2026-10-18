// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_DependencySignalAsyncAction.generated.h"

class UEdGraphPin;

/**
 * Base Blueprint node for the dependency signal async actions.
 *
 * Hides the Source Object pin while the Any Source pin is checked. Derived nodes override GetMenuActions and call
 * RegisterMenuActions<TAsyncAction>() to place the factory functions of their async action, which must have the
 * HasDedicatedAsyncNode class metadata so the generic async node does not list them as well. They extend
 * ApplyAnySourceState for their own dependent pins.
 */
UCLASS(Abstract)
class UK2Node_DependencySignalAsyncAction : public UK2Node_AsyncAction
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface

protected:
	/** Registers the factory functions of TAsyncAction so that they spawn this node class. */
	template <typename TAsyncAction>
	void RegisterMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
	{
		ActionRegistrar.RegisterClassFactoryActions<TAsyncAction>(MakeFactorySpawnerDelegate());
	}

	/** Creates the spawner for one factory function: like UK2Node_AsyncAction's, but spawning this node class. */
	FBlueprintActionDatabaseRegistrar::FMakeFuncSpawnerDelegate MakeFactorySpawnerDelegate() const;

	/**
	 * Applies the Any Source state to the dependent pins. The base hides Source Object (breaking its links) while
	 * Any Source is checked and shows it otherwise.
	 *
	 * @param bAnySource			Whether Any Source is checked.
	 * @param bNotify				Whether changes go through the schema with notifications (false during
	 *								reconstruction, when the new pins are not wired yet).
	 * @return						True if any pin changed.
	 */
	virtual bool ApplyAnySourceState(const bool bAnySource, const bool bNotify);

	/** The Any Source input pin. */
	UEdGraphPin* GetAnySourcePin() const;

	/** The Source Object input pin. */
	UEdGraphPin* GetSourceObjectPin() const;

	/** Whether an Any Source pin is checked: its default is true and nothing is linked to it. */
	static bool IsAnySourceChecked(const UEdGraphPin* AnySourcePin);

	/** Finds a pin by name among the old pins passed to ReallocatePinsDuringReconstruction. */
	static const UEdGraphPin* FindOldPin(const TArray<UEdGraphPin*>& OldPins, const FName PinName);

	/** Redraws the node after its pins changed and marks the Blueprint as modified. */
	void RefreshNode();

private:
	/** Updates the dependent pins after the Any Source pin changed. */
	void OnAnySourceChanged();
};
