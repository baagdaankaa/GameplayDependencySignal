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

	// Look for a controller in the owner chain.
	while (SourceObject)
	{
		if (AController* Controller = Cast<AController>(SourceObject))
		{
			return Controller;
		}

		if (const APawn* Pawn = Cast<APawn>(SourceObject))
		{
			return Pawn->GetController();
		}

		if (const AActor* Actor = Cast<AActor>(SourceObject))
		{
			SourceObject = Actor->GetOwner();
			continue;
		}

		SourceObject = SourceObject->GetOuter();
	}

	if (const UWorld* World = SourceObject->GetWorld())
	{
		const ENetMode NetMode = World->GetNetMode();

		if (NetMode == NM_Client || NetMode == NM_Standalone)
		{
			return World->GetFirstPlayerController();
		}
	}

	return nullptr;
}