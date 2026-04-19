// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalTypes.h"
#include "Providers/DependencySignalProviderTraits.h"

class UDependencySignalSubsystem;

struct FGameplayTagContainer;
struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class UE_API FDependencySignalAgent
{
protected:
	struct UE_API FDependencySignalRequestBuilder
	{
		UE_NONCOPYABLE(FDependencySignalRequestBuilder);

		explicit FDependencySignalRequestBuilder(const UDependencySignalSubsystem* Subsystem);

		FDependencySignalRequestBuilder& AddTag(const FGameplayTag& Tag);
		FDependencySignalRequestBuilder& AddTags(const FGameplayTagContainer& TagContainer);
		FDependencySignalRequestBuilder& AddSource(const UObject* InSourceObject);

		template <typename UserClass, typename... VarTypes>
		FDependencySignalRequestBuilder& Bind(
			UserClass* InUserObject, FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars)
		{
			PendingRequest.BindUObject(InUserObject, Func, Vars...);
			return *this;
		}

		template <typename LambdaType, typename... VarTypes>
		FDependencySignalRequestBuilder& Bind(const UObject* InUserObject, LambdaType&& InCallable, VarTypes&&... Vars)
		{
			PendingRequest.BindWeakLambda(InUserObject, Forward<LambdaType>(InCallable), Forward<VarTypes>(Vars)...);
			return *this;
		}

		FDependencySignalListenerHandle Register();

	private:
		FDependencySignalRequest PendingRequest;

		TWeakObjectPtr<const UDependencySignalSubsystem> Subsystem;
	};

	FDependencySignalRequestBuilder MakeSignalRequest(const UObject* ContextualObject) const;

	void ValidateDependency(const UObject* ContextualObject, const FGameplayTag& Tag, const bool bIsValid,
		const UObject* SourceObject = nullptr, const bool bForceRetrigger = false) const;
	void ValidateDependency(
		const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false) const;
	void ValidateDependency(const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger = false) const;

	UObject* GetDependencyObject(
		const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	template <class T = UObject>
	T* GetDependencyObject(
		const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	template <typename T>
	typename TDependencySignalTraits<T>::ReturnType GetDependency(
		const UObject* ContextualObject, UObject* SourceObject = nullptr) const;

private:
	const UDependencySignalSubsystem* GetSubsystem(const UObject* ContextualObject) const;

	mutable TWeakObjectPtr<const UDependencySignalSubsystem> CachedSubsystem;
};

template <class T>
T* FDependencySignalAgent::GetDependencyObject(
	const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject) const
{
	return Cast<T>(GetDependencyObject(ContextualObject, Tag, SourceObject));
}

template <typename T>
typename TDependencySignalTraits<T>::ReturnType FDependencySignalAgent::GetDependency(
	const UObject* ContextualObject, UObject* SourceObject) const
{
	static_assert(THasDependencySignalTraits<T>::Value,
		"T must have a valid TDependencySignalTraits specialization with ReturnType and GetTag() method. "
		"Use DECLARE_DEPENDENCY_SIGNAL_TRAIT macro or specialize manually.");

	using ReturnType = typename TDependencySignalTraits<T>::ReturnType;

	static_assert(
		TIsPointer<ReturnType>::Value && TIsDerivedFrom<typename TRemovePointer<ReturnType>::Type, UObject>::Value,
		"ReturnType must be a pointer to UObject-derived class");

	const FGameplayTag& Tag = TDependencySignalTraits<T>::GetTag();
	return GetDependencyObject<typename TRemovePointer<ReturnType>::Type>(ContextualObject, Tag, SourceObject);
}

#undef UE_API