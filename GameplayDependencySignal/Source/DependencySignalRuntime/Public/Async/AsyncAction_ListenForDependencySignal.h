// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DependencySignalTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/CancellableAsyncAction.h"
#include "Helpers/DependencySignalAgent.h"
#include "AsyncAction_ListenForDependencySignal.generated.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Async action for listening to dependency signal validity changes.
 *
 * In Blueprints, it is placed by UK2Node_ListenForDependencySignal (DependencySignalEditor module), which hides the
 * Source Object pin while Any Source is checked. HasDedicatedAsyncNode keeps the generic async node from listing it.
 */
UCLASS(MinimalAPI, BlueprintType, meta = (HasDedicatedAsyncNode))
class UAsyncAction_ListenForDependencySignal : public UCancellableAsyncAction, public FDependencySignalAgent
{
	GENERATED_BODY()

public:
	/**
	 * Asynchronously waits for the dependency signal to be broadcasted in response to the specified request.
	 *
	 * @param WorldContextObject	The object whose world context is used for subsystem access.
	 * @param Tags					The gameplay tags representing dependencies to monitor.
	 * @param bAnySource			If true, it originates from any source object, not from a specific source.
	 * @param SourceObject			Optional context object used by providers to locate dependencies.
	 * @param bDestroyAfterSuccessfulValidation	If true, automatically destroys the action after receiving a valid
	 * signal.
	 * @return					A new async action instance that can be awaited or connected to delegates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Async",
		DisplayName = "Listen For Dependency Signal Validity",
		meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_ListenForDependencySignal* ListenForDependencySignal(UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) FGameplayTagContainer Tags, const bool bAnySource,
		const UObject* SourceObject, const bool bDestroyAfterSuccessfulValidation = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
		FAsyncDependencyStateDelegate, const bool, bIsValid, UObject*, SourceObject);

	/** Delegate instance that fires when the dependency state changes. */
	UPROPERTY(BlueprintAssignable)
	FAsyncDependencyStateDelegate OnStateChanged;

protected:
	//~ Begin of UCancellableAsyncAction Interface
	UE_API virtual void Activate() override;
	UE_API virtual void SetReadyToDestroy() override;
	//~ End of UCancellableAsyncAction Interface

private:
	void HandleDependencyValidation(const bool bIsValid, UObject* InSourceObject);

	/** Weak reference to the world context used for subsystem access. */
	TWeakObjectPtr<UWorld> WorldPtr;

	/** Originating from any source object or from a specific source object. */
	bool bAnySource = false;

	/** Weak reference to the source object used for dependency resolution. */
	TWeakObjectPtr<const UObject> SourceObject;

	/** Container of gameplay tags representing the dependencies to monitor. */
	FGameplayTagContainer DependencyTags;

	/** Whether to automatically destroy this action after receiving a valid dependency signal. */
	bool bDestroyAfterSuccessfulValidation = false;

	/** Set by SetReadyToDestroy; it can run from the first callback, before Register() has returned the handle. */
	bool bFinished = false;

	/** Handle to the registered listener in the dependency signal dispatcher. */
	FDependencySignalListenerHandle ListenerHandle;
};

#undef UE_API
