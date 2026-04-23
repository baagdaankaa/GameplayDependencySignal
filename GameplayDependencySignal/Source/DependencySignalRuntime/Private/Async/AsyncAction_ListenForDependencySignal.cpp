// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Async/AsyncAction_ListenForDependencySignal.h"

UAsyncAction_ListenForDependencySignal* UAsyncAction_ListenForDependencySignal::ListenForDependencySignal(
	UObject* WorldContextObject, FGameplayTagContainer Tags, const UObject* SourceObject,
	const bool bDestroyAfterSuccessfulValidation)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UAsyncAction_ListenForDependencySignal* Action = NewObject<UAsyncAction_ListenForDependencySignal>();
	Action->WorldPtr = World;
	Action->SourceObject = SourceObject;
	Action->DependencyTags = MoveTemp(Tags);
	Action->bDestroyAfterSuccessfulValidation = bDestroyAfterSuccessfulValidation;
	Action->RegisterWithGameInstance(World);

	return Action;
}

void UAsyncAction_ListenForDependencySignal::Activate()
{
	Super::Activate();

	if (const UWorld* World = WorldPtr.Get())
	{
		ListenerHandle = MakeSignalRequest(World)
							 .AddTags(DependencyTags)
							 .AddSource(SourceObject.Get())
							 .Bind(this, &ThisClass::HandleDependencyValidation)
							 .Register();

		if (ListenerHandle.IsValid())
		{
			return;
		}
	}

	SetReadyToDestroy();
}

void UAsyncAction_ListenForDependencySignal::SetReadyToDestroy()
{
	ListenerHandle.Unregister();

	Super::SetReadyToDestroy();
}

void UAsyncAction_ListenForDependencySignal::HandleDependencyValidation(const bool bIsValid, UObject* InSourceObject)
{
	if (ShouldBroadcastDelegates())
	{
		OnStateChanged.Broadcast(bIsValid, InSourceObject);
	}

	const bool bShouldDestroy = bIsValid && bDestroyAfterSuccessfulValidation;

	if (bShouldDestroy || !OnStateChanged.IsBound())
	{
		SetReadyToDestroy();
	}
}