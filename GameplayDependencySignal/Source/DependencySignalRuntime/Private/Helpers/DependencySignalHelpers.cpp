// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Helpers/DependencySignalHelpers.h"

#include "DependencySignalDispatcher.h"
#include "GameFramework/GameStateBase.h"

void FDependencySignalHelpers::ValidateFor(const UActorComponent* Component,
	FDependencySignalDispatcher& Dispatcher, const FGameplayTag& Tag, const bool bIsValid,
	const bool bForceRetrigger)
{
	if (IsValid(Component))
	{
		ValidateFor(Component->GetOwner(), Dispatcher, Tag, bIsValid, bForceRetrigger);
	}
}

void FDependencySignalHelpers::ValidateFor(const AActor* Actor, FDependencySignalDispatcher& Dispatcher,
	const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger)
{
	if (!IsValid(Actor))
	{
		return;
	}

	Dispatcher.Validate(Tag, bIsValid, Actor, bForceRetrigger);

	if (IsLocalPlayerController(Actor) || Actor->IsA<AGameStateBase>())
	{
		Dispatcher.Validate(Tag, bIsValid, /* SourceObject */ nullptr, bForceRetrigger);
	}
}

bool FDependencySignalHelpers::IsLocalPlayerController(const AActor* Actor)
{
	const auto& GetControllerFrom = [](const AActor* TestActor) -> const AController* {
		// Look for a controller or pawn in the owner chain.
		while (TestActor)
		{
			if (const AController* C = Cast<AController>(TestActor))
			{
				return C;
			}

			if (const APawn* Pawn = Cast<APawn>(TestActor))
			{
				return Pawn->GetController();
			}

			TestActor = TestActor->GetOwner();
		}

		return nullptr;
	};

	const AController* FoundController = GetControllerFrom(Actor);
	return IsValid(FoundController) && FoundController->IsLocalPlayerController();
}