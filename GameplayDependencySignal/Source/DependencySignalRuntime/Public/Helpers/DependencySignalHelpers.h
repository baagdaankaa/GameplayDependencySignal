// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class FDependencySignalDispatcher;

struct FGameplayTag;

class UE_API FDependencySignalHelpers
{
public:
	static void ValidateFor(const UActorComponent* Component, FDependencySignalDispatcher& Dispatcher,
		const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false);
	static void ValidateFor(const AActor* Actor, FDependencySignalDispatcher& Dispatcher, const FGameplayTag& Tag,
		const bool bIsValid, const bool bForceRetrigger = false);

protected:
	static bool IsLocalPlayerController(const AActor* Actor);
};

#undef UE_API