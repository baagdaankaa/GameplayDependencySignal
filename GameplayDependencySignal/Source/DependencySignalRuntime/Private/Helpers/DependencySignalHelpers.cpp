// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Helpers/DependencySignalHelpers.h"

#include "DependencySignalDispatcher.h"
#include "GameFramework/GameStateBase.h"

void FDependencySignalHelpers::ValidateFor(const UActorComponent* Component, FDependencySignalDispatcher& Dispatcher,
	const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger)
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
		// A local actor's validation becomes its local link; its invalidation always reaches the local session.
		Dispatcher.ValidateLocalLink(Tag, bIsValid, bIsValid ? Actor : nullptr, bForceRetrigger);
	}
	else if (!bIsValid)
	{
		// The actor may have stopped being local already (e.g., a pawn's controller is cleared before
		// UnPossessed runs). It still drops the local link, but only if it is still the owner.
		Dispatcher.ValidateLocalLink(Tag, /* bIsValid */ false, Actor, bForceRetrigger);
	}
}

void FDependencySignalHelpers::UnlinkLocalFor(
	const UObject* LinkOwner, FDependencySignalDispatcher& Dispatcher, const bool bForceRetrigger)
{
	// Components validate through their owner, so the owner is the one that holds the local links.
	if (const UActorComponent* Component = Cast<UActorComponent>(LinkOwner))
	{
		LinkOwner = Component->GetOwner();
	}

	Dispatcher.UnlinkLocalSource(LinkOwner, bForceRetrigger);
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