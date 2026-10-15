// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DependencySignalBlueprintLibrary.generated.h"

struct FDependencySignalListenerHandle;
struct FGameplayTagContainer;
struct FGameplayTag;

/**
 * Blueprint library for dependency signal subsystem.
 */
UCLASS(MinimalAPI, meta = (ScriptName = "DependencySignalLibrary"))
class UDependencySignalBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------------
	// Listener Handle
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dependency Signal | Listener Handle")
	static bool IsValidDependencySignalListenerHandle(const FDependencySignalListenerHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Listener Handle")
	static void InvalidateDependencySignalListenerHandle(UPARAM(ref) FDependencySignalListenerHandle& Handle);

	// -------------------------------------------------------------------------------
	// Dependency Providers
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dependency Signal | Dependency Providers",
		meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Tag", DeterminesOutputType = "ReturnType"))
	static UObject* GetDependencySignalObject(UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTag& Tag, UObject* SourceObject,
		TSubclassOf<UObject> ReturnType);

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = "Dependency Signal | Dependency Providers",
		meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Tag", DeterminesOutputType = "ReturnType"))
	static UObject* GetLocalDependencySignalObject(UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTag& Tag, TSubclassOf<UObject> ReturnType);

	// -------------------------------------------------------------------------------
	// Validation
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dependency Signal | Validation",
		meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Tags"))
	static bool IsValidDependencySignal(const UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTagContainer& Tags,
		const UObject* SourceObject);

	UFUNCTION(BlueprintPure, Category = "Dependency Signal | Validation",
		meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Tags"))
	static bool IsValidLocalDependencySignal(const UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Validation",
		meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Tag"))
	static void ValidateDependencySignal(UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTag& Tag, const bool bIsValid,
		const UObject* SourceObject, const bool bForceRetrigger);

	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Validation",
		meta = (DefaultToSelf = "Component", AutoCreateRefTerm = "Tag"))
	static void ValidateComponentDependencySignal(const UActorComponent* Component,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger);

	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Validation",
		meta = (DefaultToSelf = "Actor", AutoCreateRefTerm = "Tag"))
	static void ValidateActorDependencySignal(const AActor* Actor,
		UPARAM(meta = (Categories = "DependencySignal")) const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger);

	/** Invalidates every local session dependency the actor published while it was local (e.g. on unpossess). */
	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Validation", meta = (DefaultToSelf = "LinkOwner"))
	static void UnlinkLocalDependencySignals(const UObject* LinkOwner, const bool bForceRetrigger);

	/** Invalidates every explicitly valid, non root dependency of the source object (None: local session). */
	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Validation",
		meta = (WorldContext = "WorldContextObject"))
	static void InvalidateDependencySignals(
		UObject* WorldContextObject, const UObject* SourceObject, const bool bForceRetrigger);
};
