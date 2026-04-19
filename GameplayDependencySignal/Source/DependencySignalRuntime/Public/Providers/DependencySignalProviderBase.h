// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DependencySignalProviderBase.generated.h"

struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Abstract base class for dependency signal providers.
 *
 * Providers are responsible for evaluating whether they can handle specific dependency tags
 * and providing the actual dependency objects when requested. The system uses providers
 * to resolve dependencies dynamically based on gameplay tags.
 *
 * Derive from this class and override Evaluate_Implementation and Provide_Implementation
 * to implement custom dependency resolution logic. Providers can optionally be implemented
 * in Blueprint for easier iteration.
 */
UCLASS(Abstract, Blueprintable)
class UE_API UDependencySignalProviderBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Checks whether this provider can handle the specified gameplay tag.
	 *
	 * The provider should return true only if it can successfully evaluate and provide
	 * a dependency for the given tag.
	 *
	 * @param Tag				The gameplay tag describing the requested dependency.
	 * @return					True if this provider can handle the tag, false otherwise.
	 *
	 * @see Provide, Evaluate_Implementation
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Dependency Signal | Provider")
	bool Evaluate(const FGameplayTag& Tag) const;
	virtual bool Evaluate_Implementation(const FGameplayTag& Tag) const;

	/**
	 * Provides an object that satisfies the dependency identified by the given tag.
	 *
	 * The provider should return a valid UObject that fulfills the dependency requirement
	 * if it previously returned true from Evaluate() for this tag.
	 *
	 * @param Tag				The gameplay tag describing the requested dependency.
	 * @param SourceObject		Optional object used as context to locate or resolve the dependency within it.
	 * @return					A UObject that fulfills the dependency, or nullptr if not available.
	 *
	 * @see Evaluate, Provide_Implementation
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Dependency Signal | Provider")
	UObject* Provide(const FGameplayTag& Tag, UObject* SourceObject) const;
	virtual UObject* Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const;

protected:
	/**
	 * Utility function to extract a controller from a source object.
	 *
	 * Attempts to retrieve a controller from the source object using various strategies:
	 * - If the object is an AController, returns it directly
	 * - If the object is an APawn, returns its controller
	 * - If the object is an AActor, returns its instigator's controller
	 * - Otherwise, returns nullptr
	 *
	 * This is a utility method for providers that need to work with controllers.
	 *
	 * @param SourceObject		The object to extract a controller from.
	 * @return					An AController if found, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Dependency Signal | Provider")
	AController* GetControllerFrom(UObject* SourceObject) const;
};

#undef UE_API
