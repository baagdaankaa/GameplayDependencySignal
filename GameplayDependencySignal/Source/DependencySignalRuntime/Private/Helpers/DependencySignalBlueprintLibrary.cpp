// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Helpers/DependencySignalBlueprintLibrary.h"

#include <Systems/DependencySignalSubsystem.h>

#include "DependencySignalTypes.h"

bool UDependencySignalBlueprintLibrary::IsValidDependencySignalListenerHandle(
	const FDependencySignalListenerHandle& Handle)
{
	return Handle.IsValid();
}

void UDependencySignalBlueprintLibrary::InvalidateDependencySignalListenerHandle(
	FDependencySignalListenerHandle& Handle)
{
	if (Handle.IsValid())
	{
		Handle.Unregister();
	}
}

bool UDependencySignalBlueprintLibrary::IsValidDependencySignal(
	const UObject* WorldContextObject, const FGameplayTagContainer& Tags, const UObject* SourceObject)
{
	const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(WorldContextObject);
	return IsValid(Subsystem) && Subsystem->IsValidDependency(Tags, SourceObject);
}

bool UDependencySignalBlueprintLibrary::IsValidLocalDependencySignal(
	const UObject* WorldContextObject, const FGameplayTagContainer& Tags)
{
	return IsValidDependencySignal(WorldContextObject, Tags, nullptr);
}

void UDependencySignalBlueprintLibrary::ValidateDependencySignal(UObject* WorldContextObject, const FGameplayTag& Tag,
	const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger)
{
	if (!Tag.IsValid())
	{
		return;
	}

	if (const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(WorldContextObject))
	{
		Subsystem->Validate(Tag, bIsValid, SourceObject, bForceRetrigger);
	}
}

void UDependencySignalBlueprintLibrary::ValidateComponentDependencySignal(
	const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger)
{
	if (!Tag.IsValid())
	{
		return;
	}

	if (const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(Component))
	{
		Subsystem->Validate(Component, Tag, bIsValid, bForceRetrigger);
	}
}

void UDependencySignalBlueprintLibrary::ValidateActorDependencySignal(
	const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger)
{
	if (!Tag.IsValid())
	{
		return;
	}

	if (const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(Actor))
	{
		Subsystem->Validate(Actor, Tag, bIsValid, bForceRetrigger);
	}
}