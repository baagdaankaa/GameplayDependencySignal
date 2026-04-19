// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalProviderTraits.h"

class UDependencySignalProviderBase;

struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class UE_API FDependencySignalProviderResolver
{
public:
	FDependencySignalProviderResolver(const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses);
	FDependencySignalProviderResolver(
		const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses, UWorld* InWorld);

	~FDependencySignalProviderResolver();

	void SetWorld(UWorld* InWorld);

	UObject* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	template <class T = UObject>
	T* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Resolves a dependency using a compile-time typed trait.
	 *
	 * This method provides type-safe, IDE-friendly dependency resolution with
	 * compile-time validation. The dependency type must have a specialized
	 * TDependencySignalTraits that defines the associated gameplay tag and return type.
	 *
	 * @tparam T				A tag type with a specialized TDependencySignalTraits.
	 * @param SourceObject		Context object used by providers to locate the dependency.
	 * @return					The resolved and cast dependency object, or nullptr if not found.
	 *
	 * @see TDependencySignalTraits, DECLARE_DEPENDENCY_SIGNAL_TRAIT
	 */
	template <typename T>
	typename TDependencySignalTraits<T>::ReturnType GetDependency(UObject* SourceObject = nullptr) const;

private:
	/** Weak reference to the world context used for dependency resolution. */
	TWeakObjectPtr<UWorld> World;

	TArray<TWeakObjectPtr<UDependencySignalProviderBase>> Providers;
};

template <class T>
T* FDependencySignalProviderResolver::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	return Cast<T>(GetDependency(Tag, SourceObject));
}

template <typename T>
typename TDependencySignalTraits<T>::ReturnType FDependencySignalProviderResolver::GetDependency(
	UObject* SourceObject) const
{
	static_assert(THasDependencySignalTraits<T>::Value,
		"T must have a valid TDependencySignalTraits specialization with ReturnType and GetTag() method. "
		"Use DECLARE_DEPENDENCY_SIGNAL_TRAIT macro or specialize manually.");

	using ReturnType = typename TDependencySignalTraits<T>::ReturnType;

	static_assert(
		TIsPointer<ReturnType>::Value && TIsDerivedFrom<typename TRemovePointer<ReturnType>::Type, UObject>::Value,
		"ReturnType must be a pointer to UObject-derived class");

	const FGameplayTag& Tag = TDependencySignalTraits<T>::GetTag();
	UObject* RawDependency = GetDependency(Tag, SourceObject);

	return Cast<typename TRemovePointer<ReturnType>::Type>(RawDependency);
}

#undef UE_API
