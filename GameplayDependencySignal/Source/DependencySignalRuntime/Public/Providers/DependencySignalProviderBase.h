// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DependencySignalProviderBase.generated.h"

struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

UCLASS(Abstract, Blueprintable)
class UE_API UDependencySignalProviderBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Checks whether this provider can handle the specified gameplay tag.
	 *
	 * @param Tag Gameplay tag describing the requested dependency.
	 * @return True if the provider can evaluate/provide for the given tag.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Dependency Signal | Provider")
	bool Evaluate(const FGameplayTag& Tag) const;
	virtual bool Evaluate_Implementation(const FGameplayTag& Tag) const;

	/**
	 * Provides an object that satisfies the dependency identified by the given tag.
	 *
	 * @param Tag Gameplay tag describing the requested dependency.
	 * @param SourceObject Object used as a context to locate or resolve the required dependency within it.
	 * @return A UObject that fulfills the dependency, or nullptr if not available.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Dependency Signal | Provider")
	UObject* Provide(const FGameplayTag& Tag, UObject* SourceObject) const;
	virtual UObject* Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const;
};

#undef UE_API
