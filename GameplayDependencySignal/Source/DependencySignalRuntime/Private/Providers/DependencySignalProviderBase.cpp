// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Providers/DependencySignalProviderBase.h"

bool UDependencySignalProviderBase::Evaluate_Implementation(const FGameplayTag& Tag) const
{
	unimplemented();
	return false;
}

UObject* UDependencySignalProviderBase::Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const
{
	unimplemented();
	return nullptr;
}

AController* UDependencySignalProviderBase::GetControllerFrom(UObject* SourceObject) const
{
	if (!ensure(IsValid(SourceObject)))
	{
		return nullptr;
	}

	{
		UObject* TempObject = SourceObject;

		// Look for a controller in the owner chain.
		while (TempObject)
		{
			if (AController* Controller = Cast<AController>(TempObject))
			{
				return Controller;
			}

			if (const APawn* Pawn = Cast<APawn>(TempObject))
			{
				return Pawn->GetController();
			}

			if (const AActor* Actor = Cast<AActor>(TempObject))
			{
				TempObject = Actor->GetOwner();
				continue;
			}

			TempObject = TempObject->GetOuter();
		}
	}

	if (const UWorld* World = SourceObject->GetWorld())
	{
		return World->GetGameInstance()->GetFirstLocalPlayerController(World);
	}

	return nullptr;
}