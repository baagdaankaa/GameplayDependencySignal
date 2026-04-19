// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "DependencySignalTypes.generated.h"

/**
 * Delegate type for dependency state change notifications.
 *
 * This delegate is invoked whenever the validity of a dependency changes,
 * passing the new validity state and the source object.
 */
DECLARE_DELEGATE_TwoParams(FDependencyStateEvent, const bool /* bIsValid */, UObject* /* SourceObject */);

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Request structure for registering a dependency signal listener.
 *
 * Use the builder-like methods (AddTag, AddTags, AddSource, Bind*) to construct
 * a request in a fluent manner before passing it to Register().
 */
USTRUCT(BlueprintType)
struct UE_API FDependencySignalRequest
{
	GENERATED_BODY()

	friend class FDependencySignalDispatcher;

	FDependencySignalRequest() = default;

	/** Adds a single gameplay tag to the list of dependencies to monitor. */
	FDependencySignalRequest& AddTag(const FGameplayTag& Tag);

	/** Adds multiple gameplay tags from a container to the list of dependencies. */
	FDependencySignalRequest& AddTags(const FGameplayTagContainer& TagContainer);

	/** Sets the source object context for this dependency request. */
	FDependencySignalRequest& AddSource(const UObject* InSourceObject);

	/** Binds a static function callback to this request. */
	template <typename... VarTypes>
	FDependencySignalRequest& BindStatic(FDependencyStateEvent::TFuncPtr<VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindStatic(Func, Vars...);
		return *this;
	}

	/** Binds a lambda callback with shared pointer safety. */
	template <typename UserClass, ESPMode Mode, typename FunctorType, typename... VarTypes>
	FDependencySignalRequest& BindSPLambda(
		const TSharedRef<UserClass, Mode>& InUserObjectRef, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSPLambda(
			InUserObjectRef, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return *this;
	}

	/** Binds a raw member function callback. */
	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindRaw(
		UserClass* InUserObject, FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindRaw(InUserObject, Func, Vars...);
		return *this;
	}

	/** Binds a shared pointer member function callback. */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	FDependencySignalRequest& BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef,
		FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSP(InUserObjectRef, Func, Vars...);
		return *this;
	}

	/** Binds a shared pointer member function callback (const pointer version). */
	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindSP(const UserClass* InUserObject,
		FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindSP(InUserObject, Func, Vars...);
		return *this;
	}

	/** Binds an Unreal Engine UFunction by name. */
	template <typename UObjectTemplate, typename... VarTypes>
	FDependencySignalRequest& BindUFunction(
		const UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindUFunction(InUserObject, InFunctionName, Vars...);
		return *this;
	}

	/** Binds a UObject member function callback. */
	template <typename UserClass, typename... VarTypes>
	FDependencySignalRequest& BindUObject(
		UserClass* InUserObject, FDependencyStateEvent::TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars)
	{
		OnDependencyStateChanged.BindUObject(InUserObject, Func, Vars...);
		return *this;
	}

	/** Binds a lambda function callback. */
	template <typename LambdaType, typename... VarTypes>
	FDependencySignalRequest& BindLambda(LambdaType&& InCallable, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindLambda(Forward<LambdaType>(InCallable), Forward<VarTypes>(Vars)...);
		return *this;
	}

	/** Binds a lambda callback with weak object reference safety. */
	template <typename LambdaType, typename... VarTypes>
	FDependencySignalRequest& BindWeakLambda(const UObject* InUserObject, LambdaType&& InCallable, VarTypes&&... Vars)
	{
		OnDependencyStateChanged.BindWeakLambda(
			InUserObject, Forward<LambdaType>(InCallable), Forward<VarTypes>(Vars)...);
		return *this;
	}

	/** Finalizes and returns this request as a rvalue for passing to Register(). */
	FDependencySignalRequest&& End()
	{
		return MoveTemp(*this);
	}

protected:
	/**
	 * Container of gameplay tags representing the dependencies to listen for.
	 *
	 * These tags identify which dependencies the listener should be notified about.
	 */
	UPROPERTY(BlueprintReadWrite, meta = (Categories = "DependencySignal"))
	FGameplayTagContainer DependencyTags;

	/**
	 * Optional source object for dependency resolution.
	 *
	 * Used by providers to locate dependencies within the context of this object.
	 * If null, providers may use a default or global context.
	 */
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<const UObject> SourceObject = nullptr;

	/**
	 * The delegate callback to invoke when dependency validity changes.
	 *
	 * This callback will be fired with the new validity state and source object
	 * whenever the registered dependencies' validity changes.
	 */
	FDependencyStateEvent OnDependencyStateChanged;
};

/**
 * Handle structure for managing registered dependency signal listeners.
 */
USTRUCT(BlueprintType)
struct UE_API FDependencySignalListenerHandle
{
	GENERATED_BODY()

	friend class FDependencySignalDispatcher;

	FDependencySignalListenerHandle() = default;

	/**
	 * Unregisters the listener associated with this handle.
	 *
	 * After calling this, the handle becomes invalid and the listener callback
	 * will no longer be invoked.
	 *
	 * @return					True if unregistration was successful, false if the handle was invalid.
	 */
	bool Unregister();

	/**
	 * Checks whether this handle is valid and registered.
	 *
	 * @return					True if the handle refers to an active listener, false otherwise.
	 */
	bool IsValid() const
	{
		return Id != 0;
	}

	/** Compares this handle with another for equality. */
	bool operator==(const FDependencySignalListenerHandle& Other) const
	{
		return Id == Other.Id;
	}

	/** Compares this handle with another for inequality. */
	bool operator!=(const FDependencySignalListenerHandle& Other) const
	{
		return Id != Other.Id;
	}

	/**
	 * Converts this handle to a string representation.
	 *
	 * @return					A string containing the handle's unique ID.
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("%llu"), Id);
	}

private:
	/** Resets this handle to an invalid state. */
	void Reset();

	/**
	 * Unique identifier for this listener handle.
	 *
	 * A value of 0 indicates an invalid handle.
	 */
	UPROPERTY(Transient)
	uint64 Id = 0;

	/**
	 * Weak reference to the dispatcher that manages this listener.
	 *
	 * Used to unregister the listener when needed.
	 */
	TWeakPtr<FDependencySignalDispatcher> Dispatcher;

	/** Helper function for using this handle as a map key. */
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