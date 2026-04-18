// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "DependencySignalTypes.generated.h"

DECLARE_DELEGATE_TwoParams(FDependencyStateEvent, const bool /* bIsValid */, UObject* /* SourceObject */);

#define UE_API DEPENDENCYSIGNALRUNTIME_API

USTRUCT(BlueprintType)
struct UE_API FDependencySignalRequest
{
	GENERATED_BODY()

	friend class FDependencySignalDispatcher;

	FDependencySignalRequest() = default;

	FDependencySignalRequest& AddTag(const FGameplayTag& Tag);
	FDependencySignalRequest& AddTags(const FGameplayTagContainer& TagContainer);
	FDependencySignalRequest& AddSource(const UObject* InSourceObject);

	template <typename... VarTypes>
	FDependencySignalRequest& BindStatic(FDependencyStateEvent::TFuncPtr<VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindStatic(Func, Vars...);
		return *this;
	}

	template <typename UserClass, ESPMode Mode, typename FunctorType, typename... VarTypes>
	FDependencySignalRequest& BindSPLambda(
		const TSharedRef<UserClass, Mode>& InUserObjectRef, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSPLambda(
			InUserObjectRef, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return *this;
	}

	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindRaw(
		UserClass* InUserObject, FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindRaw(InUserObject, Func, Vars...);
		return *this;
	}

	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	FDependencySignalRequest& BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef,
		FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSP(InUserObjectRef, Func, Vars...);
		return *this;
	}

	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindSP(const UserClass* InUserObject,
		FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSP(InUserObject, Func, Vars...);
		return *this;
	}

	template <typename UObjectTemplate, typename... VarTypes>
	FDependencySignalRequest& BindUFunction(
		const UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindUFunction(InUserObject, InFunctionName, Vars...);
		return *this;
	}

	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindUObject(
		UserClass* InUserObject, FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars)
	{
		OnDependencyStateChanged.BindUObject(InUserObject, Func, Vars...);
		return *this;
	}

	template <typename LambdaType, typename... VarTypes>
	FDependencySignalRequest& BindLambda(LambdaType&& InCallable, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindLambda(Forward<LambdaType>(InCallable), Forward<VarTypes>(Vars)...);
		return *this;
	}

	template <typename LambdaType, typename... VarTypes>
	FDependencySignalRequest& BindWeakLambda(const UObject* InUserObject, LambdaType&& InCallable, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindWeakLambda(
			InUserObject, Forward<LambdaType>(InCallable), Forward<VarTypes>(Vars)...);
		return *this;
	}

	FDependencySignalRequest&& End()
	{
		return MoveTemp(*this);
	}

protected:
	UPROPERTY(BlueprintReadWrite, meta = (Categories = "DependencySignal"))
	FGameplayTagContainer DependencyTags;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<const UObject> SourceObject = nullptr;

	FDependencyStateEvent OnDependencyStateChanged;
};

USTRUCT(BlueprintType)
struct UE_API FDependencySignalListenerHandle
{
	GENERATED_BODY()

	friend class FDependencySignalDispatcher;

	FDependencySignalListenerHandle() = default;

	bool Unregister();

	bool IsValid() const
	{
		return Id != 0;
	}

	bool operator==(const FDependencySignalListenerHandle& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FDependencySignalListenerHandle& Other) const
	{
		return Id != Other.Id;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%llu"), Id);
	}

private:
	void Reset();

	UPROPERTY(Transient)
	uint64 Id;

	TWeakPtr<FDependencySignalDispatcher> Dispatcher;

	friend uint32 GetTypeHash(const FDependencySignalListenerHandle& InHandle)
	{
		return GetTypeHash(InHandle.Id);
	}

	FDependencySignalListenerHandle(const uint64 InId, const TSharedPtr<FDependencySignalDispatcher>& InDispatcher)
		: Id(InId)
		, Dispatcher(InDispatcher)
	{
	}
};

#undef UE_API