// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Helpers/DependencySignalAgent.h"

#include "DependencySignalRuntime.h"
#include "DependencySignalTypes.h"
#include "Systems/DependencySignalSubsystem.h"

#define LOG_SUBSYSTEM_NOT_FOUND(Operation) \
	DS_UE_LOG(Error, TEXT("Unable to %s dependency. DependencySignalSubsystem not found"), TEXT(Operation))

// ---------------------------------------------------------------------------
// FDependencySignalRequestBuilder
// ---------------------------------------------------------------------------

FDependencySignalAgent::FDependencySignalRequestBuilder::FDependencySignalRequestBuilder(
	const UDependencySignalSubsystem* Subsystem)
	: Subsystem(Subsystem)
{
}

FDependencySignalAgent::FDependencySignalRequestBuilder&
FDependencySignalAgent::FDependencySignalRequestBuilder::AddTag(const FGameplayTag& Tag)
{
	PendingRequest.AddTag(Tag);
	return *this;
}

FDependencySignalAgent::FDependencySignalRequestBuilder&
FDependencySignalAgent::FDependencySignalRequestBuilder::AddTags(const FGameplayTagContainer& TagContainer)
{
	PendingRequest.AddTags(TagContainer);
	return *this;
}

FDependencySignalAgent::FDependencySignalRequestBuilder&
FDependencySignalAgent::FDependencySignalRequestBuilder::AddSource(const UObject* InSourceObject)
{
	PendingRequest.AddSource(InSourceObject);
	return *this;
}

FDependencySignalAgent::FDependencySignalRequestBuilder&
FDependencySignalAgent::FDependencySignalRequestBuilder::SetAnySource(const bool bAny)
{
	PendingRequest.SetAnySource(bAny);
	return *this;
}

FDependencySignalAgent::FDependencySignalRequestBuilder&
FDependencySignalAgent::FDependencySignalRequestBuilder::MarkAsAnySource()
{
	PendingRequest.MarkAsAnySource();
	return *this;
}

FDependencySignalListenerHandle FDependencySignalAgent::FDependencySignalRequestBuilder::Register()
{
	if (Subsystem.IsValid())
	{
		return Subsystem->Register(MoveTemp(PendingRequest));
	}

	LOG_SUBSYSTEM_NOT_FOUND("register");
	return FDependencySignalListenerHandle();
}

FScopedDependencySignalListener FDependencySignalAgent::FDependencySignalRequestBuilder::RegisterScoped()
{
	return FScopedDependencySignalListener(Register());
}

// ---------------------------------------------------------------------------
// FDependencySignalAgent
// ---------------------------------------------------------------------------

FDependencySignalAgent::FDependencySignalRequestBuilder FDependencySignalAgent::MakeSignalRequest(
	const UObject* ContextualObject) const
{
	return FDependencySignalRequestBuilder(GetSubsystem(ContextualObject));
}

void FDependencySignalAgent::ValidateDependency(const UObject* ContextualObject, const FGameplayTag& Tag,
	const bool bIsValid, const UObject* SourceObject, const bool bForceRetrigger) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(ContextualObject))
	{
		Subsystem->Validate(Tag, bIsValid, SourceObject, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_FOUND("validate");
	}
}

void FDependencySignalAgent::ValidateDependency(
	const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(Actor))
	{
		Subsystem->Validate(Actor, Tag, bIsValid, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_FOUND("validate");
	}
}

void FDependencySignalAgent::ValidateDependency(
	const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(Component))
	{
		Subsystem->Validate(Component, Tag, bIsValid, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_FOUND("validate");
	}
}

void FDependencySignalAgent::UnlinkLocalDependencies(const UObject* LinkOwner, const bool bForceRetrigger) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(LinkOwner))
	{
		Subsystem->UnlinkLocalSource(LinkOwner, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_FOUND("unlink");
	}
}

void FDependencySignalAgent::InvalidateDependencies(
	const UObject* ContextualObject, const UObject* SourceObject, const bool bForceRetrigger) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(ContextualObject))
	{
		Subsystem->InvalidateSource(SourceObject, bForceRetrigger);
	}
	else
	{
		LOG_SUBSYSTEM_NOT_FOUND("invalidate");
	}
}

UObject* FDependencySignalAgent::GetDependencyObject(
	const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject) const
{
	if (const UDependencySignalSubsystem* Subsystem = GetSubsystem(ContextualObject))
	{
		return Subsystem->GetDependency(Tag, SourceObject);
	}

	LOG_SUBSYSTEM_NOT_FOUND("get");
	return nullptr;
}

const UDependencySignalSubsystem* FDependencySignalAgent::GetSubsystem(const UObject* ContextualObject) const
{
	if (!CachedSubsystem.IsValid())
	{
		CachedSubsystem = UDependencySignalSubsystem::Get(ContextualObject);
	}

	return CachedSubsystem.Get();
}

#undef LOG_SUBSYSTEM_NOT_FOUND