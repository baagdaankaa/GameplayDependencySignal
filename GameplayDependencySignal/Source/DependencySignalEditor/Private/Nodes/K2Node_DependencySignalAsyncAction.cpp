// Copyright Bohdan Sofronii. All Rights Reserved.

#include "K2Node_DependencySignalAsyncAction.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace DependencySignal::Editor::Private
{
	/** Name of the bAnySource parameter of the async action factories. */
	const FName AnySourcePinName(TEXT("bAnySource"));

	/** Name of the SourceObject parameter of the async action factories. */
	const FName SourceObjectPinName(TEXT("SourceObject"));
} // namespace DependencySignal::Editor::Private

void UK2Node_DependencySignalAsyncAction::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin && Pin == GetAnySourcePin())
	{
		OnAnySourceChanged();
	}
}

void UK2Node_DependencySignalAsyncAction::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	// The new pins get their default values and links only after this, so read Any Source from the old pin.
	// The links of the dependent pins were already broken when Any Source was checked.
	ApplyAnySourceState(IsAnySourceChecked(FindOldPin(OldPins, DependencySignal::Editor::Private::AnySourcePinName)),
		/* bNotify */ false);
}

void UK2Node_DependencySignalAsyncAction::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin && Pin == GetAnySourcePin())
	{
		OnAnySourceChanged();
	}
}

void UK2Node_DependencySignalAsyncAction::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Abstract: derived nodes register their own async action with RegisterMenuActions. Not calling Super keeps
	// UK2Node_AsyncAction from registering every async action for this node class.
}

FBlueprintActionDatabaseRegistrar::FMakeFuncSpawnerDelegate
UK2Node_DependencySignalAsyncAction::MakeFactorySpawnerDelegate() const
{
	struct FGetMenuActionsUtils
	{
		static void SetNodeFunc(
			UEdGraphNode* NewNode, bool /* bIsTemplateNode */, TWeakObjectPtr<UFunction> FunctionPtr)
		{
			UK2Node_DependencySignalAsyncAction* AsyncTaskNode =
				CastChecked<UK2Node_DependencySignalAsyncAction>(NewNode);

			if (const UFunction* Function = FunctionPtr.Get())
			{
				const FObjectProperty* ReturnProperty =
					CastFieldChecked<FObjectProperty>(Function->GetReturnProperty());
				AsyncTaskNode->ProxyFactoryFunctionName = Function->GetFName();
				AsyncTaskNode->ProxyFactoryClass = Function->GetOuterUClass();
				AsyncTaskNode->ProxyClass = ReturnProperty->PropertyClass;
			}
		}
	};

	UClass* NodeClass = GetClass();
	return FBlueprintActionDatabaseRegistrar::FMakeFuncSpawnerDelegate::CreateLambda(
		[NodeClass](const UFunction* FactoryFunction) -> UBlueprintNodeSpawner* {
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintFunctionNodeSpawner::Create(FactoryFunction);
			check(NodeSpawner != nullptr);
			NodeSpawner->NodeClass = NodeClass;

			const TWeakObjectPtr<UFunction> FunctionPtr = MakeWeakObjectPtr(const_cast<UFunction*>(FactoryFunction));
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(
				&FGetMenuActionsUtils::SetNodeFunc, FunctionPtr);

			return NodeSpawner;
		});
}

bool UK2Node_DependencySignalAsyncAction::ApplyAnySourceState(const bool bAnySource, const bool bNotify)
{
	UEdGraphPin* SourceObjectPin = GetSourceObjectPin();
	if (!SourceObjectPin || SourceObjectPin->bHidden == bAnySource)
	{
		return false;
	}

	// A hidden pin must not keep links: they would be invisible but still feed the (ignored) value.
	if (bAnySource && SourceObjectPin->LinkedTo.Num() > 0)
	{
		GetDefault<UEdGraphSchema_K2>()->BreakPinLinks(*SourceObjectPin, bNotify);
	}

	SourceObjectPin->bHidden = bAnySource;
	return true;
}

UEdGraphPin* UK2Node_DependencySignalAsyncAction::GetAnySourcePin() const
{
	return FindPin(DependencySignal::Editor::Private::AnySourcePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DependencySignalAsyncAction::GetSourceObjectPin() const
{
	return FindPin(DependencySignal::Editor::Private::SourceObjectPinName, EGPD_Input);
}

bool UK2Node_DependencySignalAsyncAction::IsAnySourceChecked(const UEdGraphPin* AnySourcePin)
{
	// A linked value is only known at runtime, so the dependent pins stay editable then.
	return AnySourcePin && AnySourcePin->LinkedTo.Num() == 0 && AnySourcePin->GetDefaultAsString().ToBool();
}

const UEdGraphPin* UK2Node_DependencySignalAsyncAction::FindOldPin(
	const TArray<UEdGraphPin*>& OldPins, const FName PinName)
{
	UEdGraphPin* const* OldPin = OldPins.FindByPredicate([PinName](const UEdGraphPin* Pin) {
		return Pin && Pin->PinName == PinName;
	});

	return OldPin ? *OldPin : nullptr;
}

void UK2Node_DependencySignalAsyncAction::RefreshNode()
{
	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}

	if (UBlueprint* Blueprint = GetBlueprint())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void UK2Node_DependencySignalAsyncAction::OnAnySourceChanged()
{
	if (ApplyAnySourceState(IsAnySourceChecked(GetAnySourcePin()), /* bNotify */ true))
	{
		RefreshNode();
	}
}
