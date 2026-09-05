// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "DependencySignalTypes.h"
#include "Providers/DependencySignalProviderTraits.h"

class UDependencySignalSubsystem;

struct FGameplayTagContainer;
struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Base agent class for dependency signal interactions.
 *
 * Classes that need to use dependency signals should inherit from this class to access
 * its protected helper methods.
 */
class UE_API FDependencySignalAgent
{
protected:
	/**
	 * Builder pattern helper for constructing dependency signal requests.
	 *
	 * Provides a fluent interface for defining dependency signal requests with proper
	 * error handling and caching of the subsystem reference. Use MakeSignalRequest()
	 * to create an instance of this builder.
	 */
	struct UE_API FDependencySignalRequestBuilder
	{
		UE_NONCOPYABLE(FDependencySignalRequestBuilder);

		explicit FDependencySignalRequestBuilder(const UDependencySignalSubsystem* Subsystem);

		FDependencySignalRequestBuilder& AddTag(const FGameplayTag& Tag);
		FDependencySignalRequestBuilder& AddTags(const FGameplayTagContainer& TagContainer);
		FDependencySignalRequestBuilder& AddSource(const UObject* InSourceObject);

		/** Sets whether this request should listen for dependencies from any source object. */
		FDependencySignalRequestBuilder& SetAnySource(const bool bAny);

		/** Marks this request to listen for dependencies across any source object. */
		FDependencySignalRequestBuilder& MarkAsAnySource();

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

		/**
		 * Registers the built request with the dependency signal subsystem.
		 *
		 * @return				A listener handle that can be used to unregister the request later.
		 */
		FDependencySignalListenerHandle Register();

	private:
		/** The request being constructed with this builder. */
		FDependencySignalRequest PendingRequest;

		/** Weak pointer to the subsystem that will process the request. */
		TWeakObjectPtr<const UDependencySignalSubsystem> Subsystem;
	};

	/**
	 * Creates a request builder for constructing and registering dependency signal requests.
	 *
	 * @param ContextualObject	Object whose world context is used to find the subsystem.
	 * @return					A builder instance for fluent request construction.
	 */
	FDependencySignalRequestBuilder MakeSignalRequest(const UObject* ContextualObject) const;

	/**
	 * Validates a dependency signal, marking it as valid or invalid.
	 *
	 * @param ContextualObject	Object whose world context is used to find the subsystem.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param bIsValid			Whether the dependency is now valid or invalid.
	 * @param SourceObject		Optional source object for the validation scope.
	 * @param bForceRetrigger	If true, triggers listeners even if validity state didn't change.
	 */
	void ValidateDependency(const UObject* ContextualObject, const FGameplayTag& Tag, const bool bIsValid,
		const UObject* SourceObject = nullptr, const bool bForceRetrigger = false) const;

	/**
	 * Validates a dependency signal in the context of a specific actor.
	 *
	 * @param Actor				The actor providing the world context and validation scope.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param bIsValid			Whether the dependency is now valid or invalid.
	 * @param bForceRetrigger	If true, triggers listeners even if validity state didn't change.
	 */
	void ValidateDependency(
		const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false) const;

	/**
	 * Validates a dependency signal in the context of a specific actor component.
	 *
	 * @param Component			The component providing the world context and validation scope.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param bIsValid			Whether the dependency is now valid or invalid.
	 * @param bForceRetrigger	If true, triggers listeners even if validity state didn't change.
	 */
	void ValidateDependency(const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger = false) const;

	/**
	 * Retrieves a dependency object using a gameplay tag.
	 *
	 * @param ContextualObject	Object whose world context is used to find the subsystem.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param SourceObject		Optional source object for dependency resolution.
	 * @return					A UObject pointer to the dependency, or nullptr if not found.
	 */
	UObject* GetDependencyObject(
		const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Retrieves and casts a dependency object to a specific type using a gameplay tag.
	 *
	 * @tparam T				The C++ type to cast the dependency to.
	 * @param ContextualObject	Object whose world context is used to find the subsystem.
	 * @param Tag				The gameplay tag identifying the dependency.
	 * @param SourceObject		Optional source object for dependency resolution.
	 * @return					A pointer to the dependency cast to type T, or nullptr if not found.
	 */
	template <class T = UObject>
	T* GetDependencyObject(
		const UObject* ContextualObject, const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Retrieves a dependency using compile-time typed traits for type-safe access.
	 *
	 * Provides type-safe, IDE-friendly dependency resolution where the dependency type
	 * is determined at compile-time through a specialized TDependencySignalTraits.
	 *
	 * @tparam T				A tag type with a specialized TDependencySignalTraits.
	 * @param ContextualObject	Object whose world context is used to find the subsystem.
	 * @param SourceObject		Optional source object for dependency resolution.
	 * @return					The resolved dependency cast to the correct type, or nullptr.
	 */
	template <typename T>
	typename TDependencySignalTraits<T>::ReturnType GetDependency(
		const UObject* ContextualObject, UObject* SourceObject = nullptr) const;

private:
	/**
	 * Retrieves the dependency signal subsystem for a given contextual object.
	 *
	 * Caches the subsystem reference to avoid repeated lookups. Returns the cached
	 * subsystem if the context is the same as the previously cached context.
	 */
	const UDependencySignalSubsystem* GetSubsystem(const UObject* ContextualObject) const;

	/** Cached reference to the subsystem to avoid repeated lookups. */
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