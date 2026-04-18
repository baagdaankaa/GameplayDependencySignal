// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DependencySignalSubsystem.generated.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class FDependencySignalDispatcher;

struct FDependencySignalRequest;
struct FDependencySignalListenerHandle;
struct FGameplayTag;

UCLASS(MinimalAPI)
class UDependencySignalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UE_API UDependencySignalSubsystem* Get(const UObject* WorldContextObject);
	static UE_API bool HasInstance(const UObject* WorldContextObject);

	UE_API FDependencySignalListenerHandle Register(FDependencySignalRequest&& Request) const;
	UE_API void Unregister(FDependencySignalListenerHandle& ListenerHandle) const;

	UE_API void Validate(const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject = nullptr,
		const bool bForceRetrigger = false) const;
	UE_API void Validate(const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger = false) const;
	UE_API void Validate(
		const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false) const;

protected:
	//~ Begin of UWorldSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void PostInitialize() override;
	virtual void PreDeinitialize() override;
	//~ End of UWorldSubsystem Interface

private:
	TSharedPtr<FDependencySignalDispatcher> Dispatcher;
};

#undef UE_API