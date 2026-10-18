// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Async/AsyncAction_WaitForDependencySignalObject.h"

#include "DependencySignalRuntime.h"

UAsyncAction_WaitForDependencySignalObject* UAsyncAction_WaitForDependencySignalObject::WaitForDependencySignalObject(
	UObject* WorldContextObject, FGameplayTag Tag, const bool bAnySource, const UObject* SourceObject,
	TSubclassOf<UObject> ObjectClass, const bool bKeepListening)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UAsyncAction_WaitForDependencySignalObject* Action = NewObject<UAsyncAction_WaitForDependencySignalObject>();
	Action->WorldPtr = World;
	Action->DependencyTag = Tag;
	Action->bAnySource = bAnySource;
	Action->SourceObject = bAnySource ? nullptr : SourceObject;
	Action->ObjectClass = ObjectClass;
	// An any-source wait reports every source, so it always keeps listening.
	Action->bKeepListening = bKeepListening || bAnySource;
	Action->RegisterWithGameInstance(World);

	return Action;
}

void UAsyncAction_WaitForDependencySignalObject::Activate()
{
	Super::Activate();

	const UWorld* World = WorldPtr.Get();

	if (World && DependencyTag.IsValid())
	{
		ListenerHandle = MakeSignalRequest(World)
							 .AddTag(DependencyTag)
							 .SetAnySource(bAnySource)
							 .AddSource(SourceObject.Get())
							 .Bind(this, &ThisClass::HandleDependencyValidation)
							 .Register();

		if (bFinished)
		{
			// The registration callback already finished the action while the handle was still unassigned.
			ListenerHandle.Unregister();
			return;
		}

		if (ListenerHandle.IsValid())
		{
			return;
		}
	}
	else if (!DependencyTag.IsValid())
	{
		DS_UE_LOG(Warning, TEXT("Wait For Dependency Signal Object was started without a tag"));
	}

	SetReadyToDestroy();
}

void UAsyncAction_WaitForDependencySignalObject::SetReadyToDestroy()
{
	bFinished = true;
	ListenerHandle.Unregister();
	ReadyObjects.Reset();

	Super::SetReadyToDestroy();
}

void UAsyncAction_WaitForDependencySignalObject::HandleDependencyValidation(
	const bool bIsValid, UObject* InSourceObject)
{
	if (bFinished)
	{
		return;
	}

	// The dispatcher reports the local session as a null source.
	const FObjectKey SourceKey(InSourceObject);

	if (bIsValid)
	{
		UObject* Object = ResolveObject(InSourceObject);

		// Keep waiting: a later validation (or a forced retrigger) may resolve it.
		if (!Object)
		{
			return;
		}

		// A forced retrigger with the same object is not a new result.
		if (const TWeakObjectPtr<UObject>* ReadyObject = ReadyObjects.Find(SourceKey); ReadyObject
			&& ReadyObject->Get() == Object)
		{
			return;
		}

		ReadyObjects.Add(SourceKey, Object);

		if (ShouldBroadcastDelegates())
		{
			OnReady.Broadcast(Object, InSourceObject);
		}

		if (!bKeepListening)
		{
			SetReadyToDestroy();
			return;
		}
	}
	else if (TWeakObjectPtr<UObject> LostObject; ReadyObjects.RemoveAndCopyValue(SourceKey, LostObject))
	{
		if (ShouldBroadcastDelegates())
		{
			OnLost.Broadcast(LostObject.Get(), InSourceObject);
		}
	}

	if (!OnReady.IsBound() && !OnLost.IsBound())
	{
		SetReadyToDestroy();
	}
}

UObject* UAsyncAction_WaitForDependencySignalObject::ResolveObject(UObject* InSourceObject)
{
	const UWorld* World = WorldPtr.Get();
	if (!World)
	{
		return nullptr;
	}

	UObject* Object = GetDependencyObject(World, DependencyTag, InSourceObject);

	if (IsValid(Object) && (!ObjectClass || Object->IsA(ObjectClass)))
	{
		return Object;
	}

	if (!bWarnedUnresolved)
	{
		bWarnedUnresolved = true;

		if (IsValid(Object))
		{
			DS_UE_LOG(Warning, TEXT("[%s] is valid for %s, but the provider returned %s, which is not a %s"),
				*DependencyTag.ToString(), *GetNameSafe(InSourceObject), *GetNameSafe(Object),
				*GetNameSafe(ObjectClass));
		}
		else
		{
			DS_UE_LOG(Warning,
				TEXT("[%s] is valid for %s, but no provider returned an object for it; check Provider Classes in "
					 "the Dependency Signal Settings"),
				*DependencyTag.ToString(), *GetNameSafe(InSourceObject));
		}
	}

	return nullptr;
}
