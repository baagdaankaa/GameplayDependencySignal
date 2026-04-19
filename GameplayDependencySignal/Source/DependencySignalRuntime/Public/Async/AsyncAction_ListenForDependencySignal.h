// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DependencySignalTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/CancellableAsyncAction.h"
#include "Helpers/DependencySignalAgent.h"
#include "AsyncAction_ListenForDependencySignal.generated.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

UCLASS(MinimalAPI, BlueprintType)
class UAsyncAction_ListenForDependencySignal : public UCancellableAsyncAction, public FDependencySignalAgent
{
	GENERATED_BODY()

public:
	/**
	 * Asynchronously waits for the dependency signal to be broadcasted in response to the specified request.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dependency Signal | Async",
		DisplayName = "Listen For Dependency Signal Validity",
		meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_ListenForDependencySignal* ListenForDependencySignal(UObject* WorldContextObject,
		UPARAM(meta = (Categories = "DependencySignal")) FGameplayTagContainer Tags, const UObject* SourceObject,
		const bool bDestroyAfterSuccessfulValidation = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
		FAsyncDependencyStateDelegate, const bool, bIsValid, UObject*, SourceObject);
	UPROPERTY(BlueprintAssignable)
	FAsyncDependencyStateDelegate OnStateChanged;

protected:
	//~ Begin of UCancellableAsyncAction Interface
	UE_API virtual void Activate() override;
	UE_API virtual void SetReadyToDestroy() override;
	//~ End of UCancellableAsyncAction Interface

private:
	void HandleDependencyValidation(const bool bIsValid, UObject* InSourceObject);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<const UObject> SourceObject;

	FGameplayTagContainer DependencyTags;

	bool bDestroyAfterSuccessfulValidation = false;

	FDependencySignalListenerHandle ListenerHandle;
};

#undef UE_API
