// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Providers/DependencySignalProvider_Controller.h"

#include "DependencySignalTags.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerState.h"

bool UDependencySignalProvider_Controller::Evaluate_Implementation(const FGameplayTag& Tag) const
{
	return Tag.MatchesTag(DependencySignalTags::Controller);
}

UObject* UDependencySignalProvider_Controller::Provide_Implementation(
	const FGameplayTag& Tag, UObject* SourceObject) const
{
	AController* Controller = GetControllerFrom(SourceObject);

	if (!IsValid(Controller))
	{
		return nullptr;
	}

	if (Tag.MatchesTagExact(DependencySignalTags::Controller_Pawn))
	{
		return Controller->GetPawn();
	}

	if (Tag.MatchesTagExact(DependencySignalTags::Controller_PlayerState))
	{
		return Controller->GetPlayerState<APlayerState>();
	}

	return Controller;
}