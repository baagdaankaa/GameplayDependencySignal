// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DeveloperSettings.h"
#include "DependencySignalSettings.generated.h"

class UDependencySignalProviderBase;
class UWorld;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Settings for the dependency signal subsystem.
 */
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

	/**
	 * Returns true if the given world's map is in the prohibited maps list.
	 * PIE-prefixed package names are matched against their original map.
	 */
	bool IsMapProhibited(const UWorld* World) const;

	FORCEINLINE const TArray<TSubclassOf<UDependencySignalProviderBase>>& GetProviderClasses() const
	{
		return ProviderClasses;
	}

private:
	/**
	 * Whether the dependency signal subsystem should be automatically created for worlds.
	 *
	 * If true, the subsystem will be created for each world that supports it.
	 * Set to false for manually control subsystem creation.
	 */
	UPROPERTY(Config, EditAnywhere)
	bool bShouldSubsystemBeCreated = true;

	/**
	 * Maps on which the dependency signal subsystem is never created.
	 *
	 * Useful for front-end menus, loading or transition maps that do not need dependency signals.
	 * Only checked when bShouldSubsystemBeCreated is true.
	 */
	UPROPERTY(Config, EditAnywhere,
		meta = (AllowedClasses = "/Script/Engine.World", EditCondition = "bShouldSubsystemBeCreated"))
	TArray<TSoftObjectPtr<UWorld>> ProhibitedMaps;

	/**
	 * Root gameplay tags that form the base of the dependency hierarchy.
	 *
	 * These tags are used to initialize the dependency signal dispatcher and represent
	 * the top-level dependencies in the system. By default, root tags have a validity state
	 * of true, establishing a valid foundation for all child dependencies in the hierarchy.
	 * All other dependency tags should derive from these root tags.
	 */
	UPROPERTY(Config, EditAnywhere, meta = (Categories = "DependencySignal"))
	FGameplayTagContainer RootTags;

	/**
	 * List of dependency provider classes to instantiate.
	 *
	 * Each class in this array will be instantiated and added to the dependency resolution pipeline.
	 * Providers are queried to resolve dependencies.
	 */
	UPROPERTY(Config, EditAnywhere)
	TArray<TSubclassOf<UDependencySignalProviderBase>> ProviderClasses;
};

#undef UE_API