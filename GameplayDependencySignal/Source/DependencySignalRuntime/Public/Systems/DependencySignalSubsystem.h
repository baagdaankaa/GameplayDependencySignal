// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Providers/DependencySignalProviderResolver.h"
#include "DependencySignalSubsystem.generated.h"

class FDependencySignalDispatcher;

struct FDependencySignalRequest;
struct FDependencySignalListenerHandle;
struct FGameplayTagContainer;
struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

UCLASS(MinimalAPI)
class UDependencySignalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UE_API UDependencySignalSubsystem* Get(const UObject* WorldContextObject);
	static UE_API bool HasInstance(const UObject* WorldContextObject);

	UE_API FDependencySignalListenerHandle Register(FDependencySignalRequest&& Request) const;
	UE_API void Unregister(FDependencySignalListenerHandle& ListenerHandle) const;

	UE_API bool IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject = nullptr) const;
	UE_API bool IsValidDependency(const FGameplayTagContainer& Tags, const UObject* SourceObject = nullptr) const;

	UE_API void Validate(const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject = nullptr,
		const bool bForceRetrigger = false) const;
	UE_API void Validate(const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger = false) const;
	UE_API void Validate(
		const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false) const;

	UE_API UObject* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	template <class T = UObject>
	UE_API T* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

protected:
	//~ Begin of UWorldSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void PostInitialize() override;
	virtual void PreDeinitialize() override;
	//~ End of UWorldSubsystem Interface

private:
	TSharedPtr<FDependencySignalDispatcher> Dispatcher;
	TUniquePtr<FDependencySignalProviderResolver> Resolver;
};

template <class T>
T* UDependencySignalSubsystem::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	return Cast<T>(GetDependency(Tag, SourceObject));
}

#undef UE_API