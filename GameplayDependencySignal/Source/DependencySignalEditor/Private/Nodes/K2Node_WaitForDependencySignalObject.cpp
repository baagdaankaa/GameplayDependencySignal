// Copyright Bohdan Sofronii. All Rights Reserved.

#include "K2Node_WaitForDependencySignalObject.h"

#include "Async/AsyncAction_WaitForDependencySignalObject.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_DynamicCast.h"
#include "KismetCompiler.h"

namespace DependencySignal::Editor::Private
{
	/** Name of the ObjectClass parameter of WaitForDependencySignalObject. */
	const FName ObjectClassPinName(TEXT("ObjectClass"));

	/** Name of the Object parameter of FAsyncDependencyObjectDelegate (OnReady / OnLost). */
	const FName ObjectPinName(TEXT("Object"));

	/** Name of the bKeepListening parameter of WaitForDependencySignalObject. */
	const FName KeepListeningPinName(TEXT("bKeepListening"));
} // namespace DependencySignal::Editor::Private

void UK2Node_WaitForDependencySignalObject::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin && Pin == GetObjectClassPin())
	{
		OnObjectClassChanged();
	}
}

void UK2Node_WaitForDependencySignalObject::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (&Pin != GetObjectPin())
	{
		Super::GetPinHoverText(Pin, HoverTextOut);
		return;
	}

	// The pin is retyped after it was created, so build the tooltip from its current class instead of the stored one.
	const UClass* ObjectClass = Cast<UClass>(Pin.PinType.PinSubCategoryObject.Get());

	HoverTextOut = FString::Printf(
		TEXT("%s\n%s Reference\n\nThe object resolved for the tag, of the class selected on Object Class."),
		*Pin.GetDisplayName().ToString(), *GetClassTypeName(ObjectClass));
}

void UK2Node_WaitForDependencySignalObject::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	// The new Object Class pin gets its default value and links only after this, so read them from the old one.
	SetObjectPinClass(GetSelectedClass(FindOldPin(OldPins, DependencySignal::Editor::Private::ObjectClassPinName)));
}

void UK2Node_WaitForDependencySignalObject::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin && Pin == GetObjectClassPin())
	{
		OnObjectClassChanged();
	}
}

bool UK2Node_WaitForDependencySignalObject::IsConnectionDisallowed(
	const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	// Keep Listening is locked to true while Any Source is checked; a wire would bypass the read-only checkbox.
	if (MyPin && MyPin == GetKeepListeningPin() && IsAnySourceChecked(GetAnySourcePin()))
	{
		OutReason = NSLOCTEXT("DependencySignalEditor", "KeepListeningLocked",
			"Keep Listening is always on while Any Source is checked")
						.ToString();
		return true;
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

void UK2Node_WaitForDependencySignalObject::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	UEdGraphPin* ObjectPin = GetObjectPin();
	UClass* ObjectClass = GetSelectedClass(GetObjectClassPin());

	// The delegate delivers a UObject. The base expansion stores it in a temporary variable of the output pin's type
	// (even when the pin is unlinked) and cannot assign a UObject to a narrower type. So the pin goes back to UObject,
	// and its links move behind a pure cast to the selected class.
	if (ObjectPin && ObjectClass && ObjectClass != UObject::StaticClass())
	{
		UK2Node_DynamicCast* CastNode = nullptr;

		if (ObjectPin->LinkedTo.Num() > 0)
		{
			CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
			CastNode->TargetType = ObjectClass;
			CastNode->SetPurity(true);
			CastNode->AllocateDefaultPins();

			CompilerContext.MovePinLinksToIntermediate(*ObjectPin, *CastNode->GetCastResultPin());
		}

		ObjectPin->PinType.PinSubCategoryObject = UObject::StaticClass();

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (CastNode && !Schema->TryCreateConnection(ObjectPin, CastNode->GetCastSourcePin()))
		{
			CompilerContext.MessageLog.Error(
				*NSLOCTEXT("DependencySignalEditor", "CastFailed", "@@: could not cast Object to the Object Class")
					 .ToString(),
				this);
		}
	}

	Super::ExpandNode(CompilerContext, SourceGraph);
}

void UK2Node_WaitForDependencySignalObject::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	RegisterMenuActions<UAsyncAction_WaitForDependencySignalObject>(ActionRegistrar);
}

bool UK2Node_WaitForDependencySignalObject::ApplyAnySourceState(const bool bAnySource, const bool bNotify)
{
	bool bChanged = Super::ApplyAnySourceState(bAnySource, bNotify);

	UEdGraphPin* KeepListeningPin = GetKeepListeningPin();
	if (!KeepListeningPin)
	{
		return bChanged;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Keep Listening: always visible, but checked and locked while Any Source is checked, because an any-source wait
	// reports every source. Unchecking Any Source leaves it checked and editable again.
	if (KeepListeningPin->bDefaultValueIsReadOnly != bAnySource || KeepListeningPin->bNotConnectable != bAnySource)
	{
		// Read-only checkbox and no wires: the pin is fully locked. IsConnectionDisallowed backs this up.
		KeepListeningPin->bDefaultValueIsReadOnly = bAnySource;
		KeepListeningPin->bNotConnectable = bAnySource;
		bChanged = true;
	}

	if (bAnySource && KeepListeningPin->LinkedTo.Num() > 0)
	{
		Schema->BreakPinLinks(*KeepListeningPin, bNotify);
		bChanged = true;
	}

	if (bAnySource && !KeepListeningPin->GetDefaultAsString().ToBool())
	{
		if (bNotify)
		{
			Schema->TrySetDefaultValue(*KeepListeningPin, TEXT("true"));
		}
		else
		{
			KeepListeningPin->DefaultValue = TEXT("true");
		}

		bChanged = true;
	}

	return bChanged;
}

UEdGraphPin* UK2Node_WaitForDependencySignalObject::GetObjectClassPin() const
{
	return FindPin(DependencySignal::Editor::Private::ObjectClassPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WaitForDependencySignalObject::GetObjectPin() const
{
	return FindPin(DependencySignal::Editor::Private::ObjectPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_WaitForDependencySignalObject::GetKeepListeningPin() const
{
	return FindPin(DependencySignal::Editor::Private::KeepListeningPinName, EGPD_Input);
}

UClass* UK2Node_WaitForDependencySignalObject::GetSelectedClass(const UEdGraphPin* ClassPin)
{
	if (!ClassPin)
	{
		return nullptr;
	}

	if (ClassPin->LinkedTo.Num() > 0)
	{
		const UEdGraphPin* LinkedPin = ClassPin->LinkedTo[0];
		return LinkedPin ? Cast<UClass>(LinkedPin->PinType.PinSubCategoryObject.Get()) : nullptr;
	}

	return Cast<UClass>(ClassPin->DefaultObject);
}

FString UK2Node_WaitForDependencySignalObject::GetClassTypeName(const UClass* Class)
{
	if (!Class)
	{
		Class = UObject::StaticClass();
	}

	if (Class->HasAnyClassFlags(CLASS_Native))
	{
		// "A" + "Actor", "U" + "ActorComponent", ...
		return FString(Class->GetPrefixCPP()) + Class->GetName();
	}

	// Blueprint generated classes: show the Blueprint's name without the "_C" suffix.
	FString Name = Class->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

bool UK2Node_WaitForDependencySignalObject::SetObjectPinClass(UClass* InClass)
{
	UEdGraphPin* ObjectPin = GetObjectPin();
	if (!ObjectPin)
	{
		return false;
	}

	UClass* NewClass = InClass ? InClass : UObject::StaticClass();
	if (ObjectPin->PinType.PinSubCategoryObject == NewClass)
	{
		return false;
	}

	ObjectPin->PinType.PinSubCategoryObject = NewClass;
	return true;
}

void UK2Node_WaitForDependencySignalObject::OnObjectClassChanged()
{
	if (!SetObjectPinClass(GetSelectedClass(GetObjectClassPin())))
	{
		return;
	}

	// Drop links the new type no longer fits, e.g. a Pawn input after switching to PlayerState.
	if (UEdGraphPin* ObjectPin = GetObjectPin())
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		for (int32 Index = ObjectPin->LinkedTo.Num() - 1; Index >= 0; --Index)
		{
			UEdGraphPin* LinkedPin = ObjectPin->LinkedTo[Index];
			if (LinkedPin && !Schema->ArePinsCompatible(ObjectPin, LinkedPin))
			{
				Schema->BreakSinglePinLink(ObjectPin, LinkedPin);
			}
		}
	}

	RefreshNode();
}
