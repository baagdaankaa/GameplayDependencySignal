// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DeveloperSettings.h"
#include "DependencySignalSettings.generated.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Dependency Signal Settings"))
class UE_API UDependencySignalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDependencySignalSettings();

	const FGameplayTagContainer& GetRootTags() const;

private:
	UPROPERTY(Config, EditAnywhere, meta = (Categories = "DependencySignal"))
	FGameplayTagContainer RootTags;
};

#undef UE_API