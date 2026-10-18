// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Providers/DependencySignalProviderBase.h"

#include "DependencySignalRuntime.h"
#include "Engine/GameInstance.h"

bool UDependencySignalProviderBase::Evaluate_Implementation(const FGameplayTag& Tag) const
{
	DS_UE_LOG(Error, TEXT("%s must override Evaluate"), *GetNameSafe(GetClass()));
	return false;
}

UObject* UDependencySignalProviderBase::Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const
{
	DS_UE_LOG(Error, TEXT("%s must override Provide"), *GetNameSafe(GetClass()));
	return nullptr;
}

AController* UDependencySignalProviderBase::GetControllerFrom(UObject* SourceObject) const
{
	if (!IsValid(SourceObject))
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
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetFirstLocalPlayerController(World);
		}
	}

	return nullptr;
}