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

/**
 * World subsystem for managing dependency signals.
 *
 * The dependency signal subsystem is the central hub for the dependency signal system.
 * It manages the lifetime of dispatchers and providers, registers listeners, validates
 * dependencies, and provides access to resolved dependencies. The subsystem operates
 * at the world level, meaning each world has its own independent instance.
 */
UCLASS(MinimalAPI)
class UDependencySignalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Retrieves the dependency signal subsystem instance for a given world context.
	 *
	 * @param WorldContextObject	An object whose world context identifies the target world.
	 * @return						The subsystem instance for the world, or nullptr if not available.
	 */
	static UE_API UDependencySignalSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Checks whether a dependency signal subsystem instance exists for a given world context.
	 *
	 * @param WorldContextObject	An object whose world context identifies the target world.
	 * @return						True if a subsystem instance exists, false otherwise.
	 */
	static UE_API bool HasInstance(const UObject* WorldContextObject);

	/**
	 * Registers a dependency signal listener with the subsystem.
	 *
	 * The listener will be called whenever the validity state of registered dependency tags changes.
	 * The returned handle can be used to unregister the listener later.
	 *
	 * @param Request				A rvalue reference to the request containing tags, source, and callback.
	 * @return						A handle that can be used to unregister the listener.
	 */
	UE_API FDependencySignalListenerHandle Register(FDependencySignalRequest&& Request) const;

	/**
	 * Unregisters a previously registered listener using its handle.
	 *
	 * @param ListenerHandle		The handle returned by Register() (passed by reference).
	 */
	UE_API void Unregister(FDependencySignalListenerHandle& ListenerHandle) const;

	/**
	 * Checks whether a single dependency identified by a tag is currently valid.
	 *
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param SourceObject			Optional source object for validation checking.
	 * @return						True if the dependency is valid, false otherwise.
	 */
	UE_API bool IsValidDependency(const FGameplayTag& Tag, const UObject* SourceObject = nullptr) const;

	/**
	 * Checks whether all dependencies identified by tags in a container are currently valid.
	 *
	 * @param Tags					The container of gameplay tags to check.
	 * @param SourceObject			Optional source object for validation checking.
	 * @return						True if all dependencies are valid, false otherwise.
	 */
	UE_API bool IsValidDependency(const FGameplayTagContainer& Tags, const UObject* SourceObject = nullptr) const;

	/**
	 * Updates the validity state of a dependency and broadcasts the change to listeners.
	 *
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param bIsValid				Whether the dependency is now valid or invalid.
	 * @param SourceObject			Optional source object defining the validation scope.
	 * @param bForceRetrigger		If true, notifies listeners even if the state didn't change.
	 */
	UE_API void Validate(const FGameplayTag& Tag, const bool bIsValid, const UObject* SourceObject = nullptr,
		const bool bForceRetrigger = false) const;

	/**
	 * Updates the validity state of a dependency within an actor component context.
	 *
	 * The component itself provides the validation scope and world context.
	 *
	 * @param Component				The actor component providing context.
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param bIsValid				Whether the dependency is now valid or invalid.
	 * @param bForceRetrigger		If true, notifies listeners even if the state didn't change.
	 */
	UE_API void Validate(const UActorComponent* Component, const FGameplayTag& Tag, const bool bIsValid,
		const bool bForceRetrigger = false) const;

	/**
	 * Updates the validity state of a dependency within an actor context.
	 *
	 * The actor itself provides the validation scope and world context.
	 *
	 * @param Actor					The actor providing context.
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param bIsValid				Whether the dependency is now valid or invalid.
	 * @param bForceRetrigger		If true, notifies listeners even if the state didn't change.
	 */
	UE_API void Validate(
		const AActor* Actor, const FGameplayTag& Tag, const bool bIsValid, const bool bForceRetrigger = false) const;

	/**
	 * Retrieves a dependency object using a gameplay tag.
	 *
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param SourceObject			Optional source object for provider resolution.
	 * @return						The dependency object, or nullptr if not found.
	 */
	UE_API UObject* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Retrieves and casts a dependency object to a specific type using a gameplay tag.
	 *
	 * @tparam T					The C++ type to cast the dependency to.
	 * @param Tag					The gameplay tag identifying the dependency.
	 * @param SourceObject			Optional source object for provider resolution.
	 * @return						The dependency cast to type T, or nullptr if not found.
	 */
	template <class T = UObject>
	UE_API T* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	/**
	 * Retrieves a dependency using compile-time typed traits for type-safe access.
	 *
	 * Provides type-safe, IDE-friendly dependency resolution where the dependency type
	 * is determined at compile-time through a specialized TDependencySignalTraits.
	 *
	 * @tparam T					A tag type with a specialized TDependencySignalTraits.
	 * @param SourceObject			Optional source object for provider resolution.
	 * @return						The resolved dependency cast to the correct type, or nullptr.
	 *
	 * @see TDependencySignalTraits, DECLARE_DEPENDENCY_SIGNAL_TRAIT
	 */
	template <typename T>
	UE_API typename TDependencySignalTraits<T>::ReturnType GetDependency(UObject* SourceObject = nullptr) const;

protected:
	//~ Begin of UWorldSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void PostInitialize() override;
	virtual void PreDeinitialize() override;
	//~ End of UWorldSubsystem Interface

private:
	/** Pointer to the dependency signal dispatcher managing listener registration. */
	TSharedPtr<FDependencySignalDispatcher> Dispatcher;

	/** Pointer to the provider resolver that locates and instantiates providers. */
	TUniquePtr<FDependencySignalProviderResolver> Resolver;
};

template <class T>
T* UDependencySignalSubsystem::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	return Cast<T>(GetDependency(Tag, SourceObject));
}

template <typename T>
typename TDependencySignalTraits<T>::ReturnType UDependencySignalSubsystem::GetDependency(UObject* SourceObject) const
{
	if (Resolver.IsValid())
	{
		return Resolver->GetDependency<T>(SourceObject);
	}

	return nullptr;
}

#undef UE_API