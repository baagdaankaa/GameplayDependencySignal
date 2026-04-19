// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DeveloperSettings.h"
#include "DependencySignalSettings.generated.h"

class UDependencySignalProviderBase;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Dependency Signal Settings"))
class UE_API UDependencySignalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDependencySignalSettings();

	const FGameplayTagContainer& GetRootTags() const;

	FORCEINLINE bool ShouldCreateSubsystem() const
	{
		return bShouldSubsystemBeCreated;
	}

	FORCEINLINE const TArray<TSubclassOf<UDependencySignalProviderBase>>& GetProviderClasses() const
	{
		return ProviderClasses;
	}

private:
	UPROPERTY(Config, EditAnywhere)
	bool bShouldSubsystemBeCreated = true;

	UPROPERTY(Config, EditAnywhere, meta = (Categories = "DependencySignal"))
	FGameplayTagContainer RootTags;

	UPROPERTY(Config, EditAnywhere)
	TArray<TSubclassOf<UDependencySignalProviderBase>> ProviderClasses;
};

#undef UE_API