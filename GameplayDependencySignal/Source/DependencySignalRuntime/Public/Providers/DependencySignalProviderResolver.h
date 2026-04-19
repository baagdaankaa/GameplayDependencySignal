// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalProviderTraits.h"

class UDependencySignalProviderBase;

struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Resolver class for managing and using dependency signal providers.
 *
 * The provider resolver maintains a collection of provider instances and uses them
 * to resolve dependencies based on gameplay tags.
 */
class UE_API FDependencySignalProviderResolver
{
public:
	/**
	 * Constructs the resolver with a list of provider classes.
	 *
	 * Instantiates one instance of each provider class and stores them for later use.
	 *
	 * @param ProviderClasses	Array of provider classes to instantiate.
	 */
	FDependencySignalProviderResolver(const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses);

	/**
	 * Constructs the resolver with a list of provider classes and a world context.
	 *
	 * Instantiates one instance of each provider class within the given world.
	 *
	 * @param ProviderClasses	Array of provider classes to instantiate.
	 * @param InWorld			The world to use as the context for provider instantiation.
	 */
	FDependencySignalProviderResolver(
		const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses, UWorld* InWorld);

	/** Destructor that cleans up provider instances. */
	~FDependencySignalProviderResolver();

	/** Updates the world context for provider operations. */
	void SetWorld(UWorld* InWorld);

	/**
	 * Retrieves a dependency object using a gameplay tag.
	 *
	 * Queries each provider in order until one indicates it can handle the tag.
	 * Returns the object provided by the first capable provider.
	 *
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param SourceObject		Optional source object for provider resolution.
	 * @return					The dependency object, or nullptr if no provider can handle it.
	 */
	UObject* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Retrieves and casts a dependency object to a specific type using a gameplay tag.
	 *
	 * @tparam T				The C++ type to cast the dependency to.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param SourceObject		Optional source object for provider resolution.
	 * @return					The dependency cast to type T, or nullptr if not found.
	 */
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
	/**
	 * Weak pointer to the world used for dependency resolution. When no SourceObject is provided to GetDependency(),
	 * this World pointer is used as a fallback SourceObject for local session dependency resolution.
	 */
	TWeakObjectPtr<UWorld> World;

	/** Array of weak pointers to provider's CDO. */
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
