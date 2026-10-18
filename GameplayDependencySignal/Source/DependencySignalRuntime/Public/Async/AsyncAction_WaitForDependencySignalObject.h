// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DependencySignalTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/CancellableAsyncAction.h"
#include "Helpers/DependencySignalAgent.h"
#include "AsyncAction_WaitForDependencySignalObject.generated.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Async action that waits until a dependency tag is valid and outputs the object a provider resolves for it.
 *
 * Unlike Listen For Dependency Signal Validity, it resolves the object for you. OnReady fires only when the tag is
 * valid AND a provider returned an object of the requested class.
 *
 * In Blueprints it is placed by UK2Node_WaitForDependencySignalObject (DependencySignalEditor module), which types
 * the Object pin with the selected Object Class. HasDedicatedAsyncNode keeps the generic async node from listing it.
 */
UCLASS(MinimalAPI, BlueprintType, meta = (HasDedicatedAsyncNode))
class UAsyncAction_WaitForDependencySignalObject : public UCancellableAsyncAction, public FDependencySignalAgent
{
	GENERATED_BODY()

public:
	/**
	 * Waits until the dependency is valid and resolves its object.
	 *
	 * @param WorldContextObject	The object whose world context is used for subsystem access.
	 * @param Tag					The dependency tag to wait for.
	 * @param bAnySource			If true, waits for the tag on every source (SourceObject is ignored), reports
	 *								each source separately through the Source output and keeps listening.
	 * @param SourceObject			Optional source the dependency is scoped to (None: local session).
	 * @param ObjectClass			Class the resolved object must have; None accepts any object.
	 * @param bKeepListening		If false, the action ends after the first OnReady. If true, it keeps
	 *								running, fires OnLost when the dependency becomes invalid and OnReady
	 *								again when it returns. Always true with bAnySource, which reports every
	 *								source separately.
	 * @return						A new async action instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Async",
		DisplayName = "Wait For Dependency Signal Object",
		meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_WaitForDependencySignalObject* WaitForDependencySignalObject(
		UObject* WorldContextObject, UPARAM(meta = (Categories = "DependencySignal")) FGameplayTag Tag,
		const bool bAnySource, const UObject* SourceObject, TSubclassOf<UObject> ObjectClass,
		const bool bKeepListening = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
		FAsyncDependencyObjectDelegate, UObject*, Object, UObject*, Source);

	/**
	 * Fires when the dependency is valid and its object was resolved; Object is of the requested class.
	 * Source is the source the dependency belongs to (None: local session).
	 */
	UPROPERTY(BlueprintAssignable)
	FAsyncDependencyObjectDelegate OnReady;

	/**
	 * Fires when the dependency becomes invalid after OnReady (only with bKeepListening).
	 * Object is the last object reported for that Source.
	 */
	UPROPERTY(BlueprintAssignable)
	FAsyncDependencyObjectDelegate OnLost;

protected:
	//~ Begin of UCancellableAsyncAction Interface
	UE_API virtual void Activate() override;
	UE_API virtual void SetReadyToDestroy() override;
	//~ End of UCancellableAsyncAction Interface

private:
	void HandleDependencyValidation(const bool bIsValid, UObject* InSourceObject);

	/** Resolves the object for the tag on a source and checks its class; logs a warning once when that fails. */
	UObject* ResolveObject(UObject* InSourceObject);

	/** Weak reference to the world used for subsystem access. */
	TWeakObjectPtr<UWorld> WorldPtr;

	/** The dependency tag to wait for. */
	FGameplayTag DependencyTag;

	/** Weak reference to the source the dependency is scoped to. */
	TWeakObjectPtr<const UObject> SourceObject;

	/** Class the resolved object must have; null accepts any object. */
	UPROPERTY()
	TSubclassOf<UObject> ObjectClass;

	/**
	 * Sources whose OnReady fired and whose OnLost has not fired since, with the object passed to OnReady.
	 * Holds at most one entry without bAnySource. The local session uses an empty key.
	 */
	TMap<FObjectKey, TWeakObjectPtr<UObject>> ReadyObjects;

	/** Whether to wait for the tag on every source instead of SourceObject. */
	bool bAnySource = false;

	/** Whether to keep running after the first OnReady. */
	bool bKeepListening = false;

	/** Whether the "valid but not resolved" warning was already logged. */
	bool bWarnedUnresolved = false;

	/** Set by SetReadyToDestroy; it can run from the first callback, before Register() has returned the handle. */
	bool bFinished = false;

	/** Handle to the registered listener in the dependency signal dispatcher. */
	FDependencySignalListenerHandle ListenerHandle;
};

#undef UE_API
