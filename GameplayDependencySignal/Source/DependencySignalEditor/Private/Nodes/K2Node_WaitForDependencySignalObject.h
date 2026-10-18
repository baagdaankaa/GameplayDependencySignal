// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_DependencySignalAsyncAction.h"
#include "K2Node_WaitForDependencySignalObject.generated.h"

class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;

/**
 * Blueprint node for UAsyncAction_WaitForDependencySignalObject::WaitForDependencySignalObject.
 *
 * On top of the base node (Source Object hidden while Any Source is checked):
 * - Types the Object output pin (shared by OnReady and OnLost) with the class selected on the Object Class pin, so no
 *   cast is needed after it. When Object Class is linked, the class of the linked pin is used; when it is empty, the
 *   pin stays a plain Object. At compile time the delegate still delivers a UObject, so the expansion inserts a pure
 *   cast to the selected class; the action only fires OnReady with an object of that class, so it always succeeds.
 * - Locks Keep Listening to true while Any Source is checked.
 */
UCLASS()
class UK2Node_WaitForDependencySignalObject : public UK2Node_DependencySignalAsyncAction
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsConnectionDisallowed(
		const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface

protected:
	//~ Begin UK2Node_DependencySignalAsyncAction Interface

	/**
	 * Also locks Keep Listening while Any Source is checked: it stays visible, but is forced to true, its default is
	 * read-only, its links are broken and it cannot be connected.
	 */
	virtual bool ApplyAnySourceState(const bool bAnySource, const bool bNotify) override;
	//~ End UK2Node_DependencySignalAsyncAction Interface

private:
	/** The Object Class input pin. */
	UEdGraphPin* GetObjectClassPin() const;

	/** The Object output pin of OnReady / OnLost. */
	UEdGraphPin* GetObjectPin() const;

	/** The Keep Listening input pin. */
	UEdGraphPin* GetKeepListeningPin() const;

	/** The class selected on a class pin: the linked pin's class when linked, otherwise its default value. */
	static UClass* GetSelectedClass(const UEdGraphPin* ClassPin);

	/** The C++ style name of a class: "AActor" for native classes, "BP_Hero" for Blueprint classes. */
	static FString GetClassTypeName(const UClass* Class);

	/** Types the Object output pin with InClass (UObject when null). Returns true if the type changed. */
	bool SetObjectPinClass(UClass* InClass);

	/** Re-types the Object output pin after the Object Class pin changed, dropping links that no longer fit. */
	void OnObjectClassChanged();
};
