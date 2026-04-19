// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Systems/DependencySignalSubsystem.h"

#include "DependencySignalDispatcher.h"
#include "DependencySignalRuntime.h"
#include "Helpers/DependencySignalHelpers.h"
#include "Settings/DependencySignalSettings.h"

#define LOG_SUBSYSTEM_NOT_READY(Operation) \
	DS_UE_LOG(Error, TEXT("Unable to %s dependency. DependencySignalSubsystem not initialized"), TEXT(Operation))

UDependencySignalSubsystem* UDependencySignalSubsystem::Get(const UObject* WorldContextObject)
{
	return UWorld::GetSubsystem<UDependencySignalSubsystem>(
		GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert));
}

bool UDependencySignalSubsystem::HasInstance(const UObject* WorldContextObject)
{
	return IsValid(UDependencySignalSubsystem::Get(WorldContextObject));
}

FDependencySignalListenerHandle UDependencySignalSubsystem::Register(FDependencySignalRequest&& Request) const
{
	if (Dispatcher.IsValid())
	{
		return Dispatcher->Register(MoveTemp(Request));
	}

	LOG_SUBSYSTEM_NOT_READY("register");
	return FDependencySignalListenerHandle();
}

void UDependencySignalSubsystem::Unregister(FDependencySignalListenerHandle& ListenerHandle) const
{
	if (Dispatcher.IsValid())
	{
		Dispatcher->Unregister(ListenerHandle);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_READY("unregister");
	}
}

bool UDependencySignalSubsystem::IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject) const
{
	return Dispatcher.IsValid() && Dispatcher->IsValidDependency(Tag, SourceObject);
}

bool UDependencySignalSubsystem::IsValidDependency(const FGameplayTagContainer& Tags, const UObject* SourceObject) const
{
	return Dispatcher.IsValid() && Dispatcher->IsValidDependency(Tags, SourceObject);
}

void UDependencySignalSubsystem::Validate(
	const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger) const
{
	if (Dispatcher.IsValid())
	{
		Dispatcher->Validate(Tag, bIsValid, SourceObject, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_READY("validate");
	}
}

void UDependencySignalSubsystem::Validate(
	const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger) const
{
	if (Dispatcher.IsValid())
	{
		FDependencySignalHelpers::ValidateFor(Component, *Dispatcher, Tag, bIsValid, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_READY("validate");
	}
}

void UDependencySignalSubsystem::Validate(
	const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger) const
{
	if (Dispatcher.IsValid())
	{
		FDependencySignalHelpers::ValidateFor(Actor, *Dispatcher, Tag, bIsValid, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_READY("validate");
	}
}

UObject* UDependencySignalSubsystem::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	if (Resolver.IsValid())
	{
		return Resolver->GetDependency(Tag, SourceObject);
	}

	LOG_SUBSYSTEM_NOT_READY("get");
	return nullptr;
}

bool UDependencySignalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return GetDefault<UDependencySignalSettings>()->ShouldCreateSubsystem() && Super::ShouldCreateSubsystem(Outer);
}

bool UDependencySignalSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// clang-format off
	return WorldType == EWorldType::Game 
		|| WorldType == EWorldType::PIE 
		|| WorldType == EWorldType::GamePreview
		|| WorldType == EWorldType::GameRPC;
	// clang-format on
}

void UDependencySignalSubsystem::PostInitialize()
{
	Super::PostInitialize();

	const UDependencySignalSettings* Settings = GetDefault<UDependencySignalSettings>();

	if (!Dispatcher.IsValid())
	{
		Dispatcher = MakeShared<FDependencySignalDispatcher>(Settings->GetRootTags(), this);
	}

	if (!Resolver.IsValid())
	{
		Resolver = MakeUnique<FDependencySignalProviderResolver>(Settings->GetProviderClasses(), GetWorld());
	}

	if (Resolver.IsValid() && Dispatcher.IsValid())
	{
		DS_UE_LOG(Log, TEXT("DependencySignalSubsystem successfully initialized"));
	}
	else
	{
		DS_UE_LOG(Error, TEXT("DependencySignalSubsystem was NOT successfully initialized"));
	}
}

void UDependencySignalSubsystem::PreDeinitialize()
{
	if (Dispatcher.IsValid())
	{
		Dispatcher.Reset();
	}

	if (Resolver.IsValid())
	{
		Resolver.Reset();
	}

	if (!Resolver.IsValid() && !Dispatcher.IsValid())
	{
		DS_UE_LOG(Log, TEXT("DependencySignalSubsystem successfully deinitialized"));
	}
	else
	{
		DS_UE_LOG(Error, TEXT("Some DependencySignalSubsystem's data has not been cleared"));
	}

	Super::PreDeinitialize();
}

#undef LOG_SUBSYSTEM_NOT_READY