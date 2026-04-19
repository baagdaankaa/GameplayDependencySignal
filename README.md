# Gameplay Dependency Signal

## Table of Contents

- [Why Use It?](#why-use-it)
  - [What it helps with](#what-it-helps-with)
  - [In practice](#in-practice)
- [Features](#features)
- [Built-in Tags](#built-in-tags)
- [Core Concept](#core-concept)
- [Understanding Hierarchy](#understanding-hierarchy)
- [Understanding `SourceObject`](#understanding-sourceobject)
  - [Example](#example)
  - [When to use a real `SourceObject`](#when-to-use-a-real-sourceobject)
  - [When to use `nullptr`](#when-to-use-nullptr)
- [Built-in Provider Behavior](#built-in-provider-behavior)
- [Interaction in Blueprints](#interaction-in-blueprints)
  - [Waiting for Dependencies](#waiting-for-dependencies)
  - [Validating Dependencies](#validating-dependencies)
  - [Checking Validity](#checking-validity)
  - [Getting the Object](#getting-the-object)
  - [Listener Handle Helpers](#listener-handle-helpers)
- [Interaction in C++](#interaction-in-c)
- [`FDependencySignalAgent` vs `UDependencySignalSubsystem`](#fdependencysignalagent-vs-udependencysignalsubsystem)
  - [Use `FDependencySignalAgent` when](#use-fdependencysignalagent-when)
  - [Use `UDependencySignalSubsystem` when](#use-udependencysignalsubsystem-when)
  - [Short recommendation](#short-recommendation)
- [Interaction in C++ Examples](#interaction-in-c-examples)
  - [1. Registering a Listener with `FDependencySignalAgent`](#1-registering-a-listener-with-fdependencysignalagent)
  - [2. Registering for multiple dependencies at once](#2-registering-for-multiple-dependencies-at-once)
  - [3. Registering with 2-3 tags from different hierarchy levels](#3-registering-with-2-3-tags-from-different-hierarchy-levels)
  - [4. Registering with a lambda](#4-registering-with-a-lambda)
  - [5. Direct registration through `UDependencySignalSubsystem`](#5-direct-registration-through-udependencysignalsubsystem)
  - [6. Validating a dependency](#6-validating-a-dependency)
  - [7. Using explicit `SourceObject`](#7-using-explicit-sourceobject)
  - [8. Using local dependencies on the client with `nullptr`](#8-using-local-dependencies-on-the-client-with-nullptr)
  - [9. Type-safe dependency traits](#9-type-safe-dependency-traits)
- [Extending Dependency Resolution with `UDependencySignalProviderBase`](#extending-dependency-resolution-with-udependencysignalproviderbase)
  - [When to create a custom provider](#when-to-create-a-custom-provider)
  - [Minimal example](#minimal-example)
  - [Registration](#registration)
  - [Why providers are useful](#why-providers-are-useful)
- [Configuration](#configuration)
  - [Available settings](#available-settings)
- [Practical Tips](#practical-tips)
- [Summary](#summary)

**Gameplay Dependency Signal** is a lightweight reactive dependency system for Unreal Engine that helps gameplay code, UI, and other runtime systems wait for objects and states in a clean, event-driven way.

Instead of manually checking whether a `Controller`, `Pawn`, `PlayerState`, or any custom gameplay dependency is ready, you register interest in one or more dependency tags and receive a callback when the dependency becomes valid or invalid.

It is especially useful in multiplayer projects, where object availability is often delayed by spawning order, possession flow, replication, or ownership changes.

---

## Why Use It?

Gameplay systems rarely become ready at the same time.

A widget may need a local controller and pawn. A gameplay component may depend on a possessed pawn. A feature may require both `PlayerState` and another custom tag before it can initialize. Handling this with `BeginPlay`, `Tick`, timers, delays, or chains of null checks quickly becomes noisy and fragile.

**Gameplay Dependency Signal** gives you a single place to express readiness requirements and react only when those requirements are actually satisfied.

### What it helps with
- Waiting for replicated objects on clients without polling.
- Decoupling systems that should not know who creates or owns the dependency.
- Tracking readiness per object context, not only globally.
- Reacting to both **valid** and **invalid** transitions.
- Organizing dependencies with hierarchical gameplay tags.

### In practice
Instead of writing:
- "wait until controller exists"
- "wait until pawn is possessed"
- "wait until player state is replicated"
- "try again next tick"

you can simply:
- register a dependency request,
- listen for validity changes,
- resolve the dependency object when it becomes available.

---

## Features

- **World Subsystem-based runtime**
  - Each supported world gets its own `UDependencySignalSubsystem`.
  - The subsystem manages listeners, validation state, and dependency providers.

- **Reactive listeners**
  - Register for one tag or multiple tags.
  - Receive callbacks when the combined validity changes.
  - Works in both C++ and Blueprints.

- **Hierarchical gameplay tag support**
  - Dependencies are organized through gameplay tags.
  - A child tag depends on the validity of its parent hierarchy.
  - Example:
    - `DependencySignal`
    - `DependencySignal.Controller`
    - `DependencySignal.Controller.Pawn`
    - `DependencySignal.Controller.PlayerState`

- **Scoped dependencies via `SourceObject`**
  - The same tag can be tracked separately for different source objects.
  - This lets you express things like:
    - "Pawn for this specific controller"
    - "Dependencies for this actor"
    - "Dependencies for my local client context"

- **Local dependency support**
  - When used on a local client, the system supports local dependency resolution with `SourceObject = nullptr`.
  - This is very convenient for client-side UI and local player flows.

- **Validation and invalidation**
  - Dependencies can become valid or invalid at runtime.
  - Listeners are notified when the effective state changes.
  - You can also force retrigger notifications.

- **Provider-based dependency resolution**
  - Tags are resolved to actual `UObject` instances through provider classes.
  - Built-in support already includes controller-related dependencies.

- **Blueprint support**
  - Async node for listening to dependency validity.
  - Blueprint library functions for validation and object retrieval.
  - Helper functions for local dependency checks.

- **Type-safe C++ access**
  - Use traits to map tags to concrete C++ types and avoid manual casting.

---

## Built-in Tags

The runtime currently defines these built-in tags:

- `DependencySignal`
- `DependencySignal.Controller`
- `DependencySignal.Controller.Pawn`
- `DependencySignal.Controller.PlayerState`

These are intended as a default dependency hierarchy for controller-related flows.

---

## Core Concept

At the center of the system is a simple idea:

> A dependency is identified by a **Gameplay Tag** and optionally scoped by a **SourceObject**.

For example:
- `DependencySignal.Controller.Pawn` can mean:
  - "the pawn belonging to this specific controller" when `SourceObject` is a controller,
  - or "the local player's pawn" when `SourceObject` is `nullptr` on a client/local context.

A dependency becomes useful when two things are true:

1. its **validity state** is marked as valid,
2. a provider can **resolve** the tag into an actual object.

These are related, but not identical:
- **validation** tells the system whether the dependency is considered ready;
- **provider resolution** tells the system how to obtain the actual object for that tag.

---

## Understanding Hierarchy

Dependency tags are hierarchical.

For example:

- `DependencySignal`
- `DependencySignal.Controller`
- `DependencySignal.Controller.Pawn`

If a parent dependency is not effectively valid, a child dependency cannot be effectively valid either.

That means hierarchical tags let you represent readiness naturally:
- controller root readiness,
- then controller itself,
- then controller-owned pawn,
- then anything even more specific under that branch.

This makes it easier to organize initialization rules without scattering logic across many systems.

---

## Understanding `SourceObject`

`SourceObject` defines **which dependency context** you are talking about.

Think of it as the "owner" or "scope" of the dependency state.

### Example
`DependencySignal.Controller.Pawn` can exist:
- for `PlayerController A`
- for `PlayerController B`
- for local client context (`nullptr`)
- for some actor chain that resolves to a controller

These are not necessarily the same dependency entry.

### When to use a real `SourceObject`
Use a real source when your dependency is tied to a specific object, for example:
- a specific controller,
- a pawn,
- an actor owned by a controller,
- a component attached to a controlled actor.

### When to use `nullptr`
Use `nullptr` when you want **local dependencies**.

This is particularly useful on the **client**:
- if you are on a local client, you can pass `nullptr` as `SourceObject`,
- then the system can resolve dependencies using the local world/player context,
- which is very useful for client-only UI.

For example, a HUD, menu, or widget may want the local controlling pawn without caring about a specific replicated source object. In that case, using `nullptr` allows you to query the local dependency context directly.

This is a good fit for UI that depends on the client's local player state, local controller, or controlling pawn.

---

## Built-in Provider Behavior

The runtime includes a built-in controller provider that handles the controller dependency branch.

It can resolve:
- `DependencySignal.Controller` → controller
- `DependencySignal.Controller.Pawn` → controller's pawn
- `DependencySignal.Controller.PlayerState` → controller's player state

The provider attempts to find a controller from the supplied source context. If needed, it walks through actor/owner relationships, and for local client scenarios it can fall back to the first local player controller.

---

## Interaction in Blueprints

Blueprints can work with the system in three main ways:
- wait for dependency state changes,
- validate or invalidate dependencies,
- resolve dependency objects.

Before using the nodes, it helps to decide whether your dependency is:
- **scoped** to a specific `SourceObject`, or
- **local** to the current client, using `nullptr`.

### Waiting for Dependencies

Use the async action:

- `Listen For Dependency Signal Validity`

This node listens for one or more tags and broadcasts when the effective validity changes.

Good use cases:
- waiting for controller + pawn before initializing UI,
- reacting when a pawn becomes invalid after unpossession,
- waiting for replicated readiness on clients.

### Validating Dependencies

Use one of the validation helpers:
- `Validate Dependency Signal`
- `Validate Actor Dependency Signal`
- `Validate Component Dependency Signal`

These let you mark a dependency as valid or invalid.

### Checking Validity

Use:
- `Is Valid Dependency Signal`
- `Is Valid Local Dependency Signal`

These are useful when you need an immediate check instead of subscribing.

### Getting the Object

Use:
- `Get Dependency Signal Object`
- `Get Local Dependency Signal Object`

These resolve the dependency object for a tag.

For UI on the local client, `Get Local Dependency Signal Object` is often the most convenient option because it uses the local context.

### Listener Handle Helpers

If you store a listener handle in Blueprint, you can manage it with:
- `Is Valid Dependency Signal Listener Handle`
- `Invalidate Dependency Signal Listener Handle`

This is useful when you want to stop listening manually.

---

## Interaction in C++

## `FDependencySignalAgent` vs `UDependencySignalSubsystem`

There are two main ways to work with the system in C++.

### Use `FDependencySignalAgent` when
- you want ergonomic helper methods;
- you are implementing logic inside a class that frequently talks to the dependency system;
- you want fluent request building through `MakeSignalRequest(...)`;
- you want less boilerplate when validating and resolving dependencies.

**Why it is convenient**
- cached subsystem lookup;
- fluent request builder;
- helper methods like `ValidateDependency(...)`;
- helper methods like `GetDependencyObject(...)`.

This is usually the most user-friendly option for gameplay classes and async-style consumers.

### Use `UDependencySignalSubsystem` when
- you want direct explicit access to the underlying runtime API;
- you do not want to inherit from `FDependencySignalAgent`;
- you are writing lower-level systems, framework code, or utility code;
- you want to register from a manually constructed `FDependencySignalRequest`.

**Why it is useful**
- explicit and direct;
- easier for shared subsystems/services;
- better if you prefer keeping dependency access centralized.

### Short recommendation
- For **most gameplay classes**, use **`FDependencySignalAgent`**.
- For **centralized runtime code** or explicit subsystem-oriented architecture, use **`UDependencySignalSubsystem`**.

---

## Interaction in C++ Examples

### 1. Registering a Listener with `FDependencySignalAgent`

A typical usage pattern is to inherit from `FDependencySignalAgent` inside your gameplay class.
To react to dependency changes in code, use the `FDependencySignalAgent` or the subsystem directly.

```cpp
class UMyWidgetController : public UObject, protected FDependencySignalAgent 
{ 
    GENERATED_BODY()

public: 
    void Initialize(); 
    void Shutdown();
  
private: 
    void HandlePawnStateChanged(bool bIsValid, UObject* SourceObject);
  
    FDependencySignalListenerHandle PawnListenerHandle;
};

void UMyWidgetController::Initialize() 
{ 
    PawnListenerHandle = MakeSignalRequest(this)
        .AddTag(DependencySignalTags::Controller_Pawn)
        .AddSource(nullptr) // local client dependency context
        .Bind(this, &UMyWidgetController::HandlePawnStateChanged)
        .Register();
}

void UMyWidgetController::Shutdown() 
{ 
    PawnListenerHandle.Unregister(); 
}

void UMyWidgetController::HandlePawnStateChanged(bool bIsValid, UObject* SourceObject) 
{ 
    if (!bIsValid) 
    { 
        return; 
    }

    APawn* LocalPawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
    
    // Initialize UI with LocalPawn...
}
```

### 2. Registering for multiple dependencies at once

A listener can depend on multiple tags. The callback represents the **combined state** of the request.

```cpp 
void UMyFeature::BeginListen() 
{     
    ListenerHandle = MakeSignalRequest(this)
        .AddTag(DependencySignalTags::Controller)
        .AddTag(DependencySignalTags::Controller_PlayerState)
        .AddSource(GetOwningPlayer())
        .Bind(this, &UMyFeature::HandleDependenciesChanged)
        .Register();
}

void UMyFeature::HandleDependenciesChanged(bool bIsValid, UObject* SourceObject) 
{ 
    if (!bIsValid) 
    {
        // At least one dependency is no valid 
        return; 
    }

    AController* Controller = GetDependencyObject<AController>(
        this, DependencySignalTags::Controller, GetOwningPlayer());
    APlayerState* PlayerState = GetDependencyObject<APlayerState>(
        this, DependencySignalTags::Controller_PlayerState, GetOwningPlayer());
    
    // Safe initialization path...
}

void UMyFeature::StopListen() 
{ 
    ListenerHandle.Unregister(); 
}
```

### 3. Registering with 2-3 tags from different hierarchy levels

Because the system supports hierarchical tags, you can register tags from different points in the same branch.

For example, this request listens to:
- `DependencySignal.Controller`
- `DependencySignal.Controller.Pawn`
- `DependencySignal.Controller.PlayerState`

```cpp 
void UMyHUDPresenter::Start() 
{ 
    ListenerHandle = MakeSignalRequest(this) 
        .AddTag(DependencySignalTags::Controller) 
        .AddTag(DependencySignalTags::Controller_Pawn) 
        .AddTag(DependencySignalTags::Controller_PlayerState) 
        .AddSource(nullptr) // use local client context for HUD/UI 
        .Bind(this, &UMyHUDPresenter::HandleControllerChainChanged) 
        .Register(); 
}

void UMyHUDPresenter::HandleControllerChainChanged(bool bIsValid, UObject* SourceObject) 
{ 
    if (!bIsValid) 
    { 
        // One of the required dependencies is missing or became invalid. 
        return;
    }

    AController* Controller = GetDependencyObject<AController>(this, DependencySignalTags::Controller);
    APawn* Pawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
    APlayerState* PlayerState = GetDependencyObject<APlayerState>(this, DependencySignalTags::Controller_PlayerState);

    // Update HUD using local controller, pawn and player state...
}

void UMyHUDPresenter::Stop() 
{ 
    ListenerHandle.Unregister(); 
}
```

This pattern is useful when a system should only initialize after several related objects are ready.

### 4. Registering with a lambda

You can also bind a weak lambda through the agent builder.

```cpp
void UMyObject::StartListening() 
{ 
    ListenerHandle = MakeSignalRequest(this) 
        .AddTag(DependencySignalTags::Controller_Pawn) 
        .AddSource(nullptr) 
        .Bind(this, [this](bool bIsValid, UObject* SourceObject) 
        { 
            if (!bIsValid) 
            { 
                return; 
            }

            APawn* LocalPawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
            
            // React to LocalPawn...
        })
        .Register();
}
void UMyObject::StopListening() 
{ 
    ListenerHandle.Unregister(); 
}
```

### 5. Direct registration through `UDependencySignalSubsystem`

If you do not want to inherit from `FDependencySignalAgent`, you can talk directly to the subsystem.

```cpp
void UMyManager::StartListening() 
{ 
    if (UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this)) 
    { 
        FDependencySignalRequest Request
          .AddTag(DependencySignalTags::Controller)
          .AddTag(DependencySignalTags::Controller_Pawn)
          .BindUObject(this, &UMyManager::HandleDependencyChanged);
        ListenerHandle = Subsystem->Register(Request.End());
    }
}

void UMyManager::StopListening() 
{ 
    if (UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this)) 
    { 
        Subsystem->Unregister(ListenerHandle); 
    } 
}

void UMyManager::HandleDependencyChanged(bool bIsValid, UObject* SourceObject) 
{ 
    if (!bIsValid) 
    { 
        return; 
    }

    if (UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this))
    {
        APawn* Pawn = Subsystem->GetDependency<APawn>(DependencySignalTags::Controller_Pawn);
        // Use Pawn...
    }
}
```

### 6. Validating a dependency

When an object becomes ready, validate the corresponding tag.

```cpp
void AMyPlayerController::OnPossess(APawn* InPawn) 
{ 
    Super::OnPossess(InPawn);

    ValidateDependency(this, DependencySignalTags::Controller_Pawn, IsValid(InPawn));
}
```

You can also invalidate it later:

```cpp
void AMyPlayerController::OnUnPossess() 
{ 
    ValidateDependency(this, DependencySignalTags::Controller_Pawn, false);
    Super::OnUnPossess();
}
```

### 7. Using explicit `SourceObject`

If you want the dependency state to belong to a specific object context, pass that source explicitly.

```cpp 
void UMyActorBoundFeature::StartForActor(AActor* TargetActor) 
{ 
    check(TargetActor);
    ListenerHandle = MakeSignalRequest(this)
        .AddTag(DependencySignalTags::Controller_Pawn)
        .AddSource(TargetActor)
        .Bind(this, &UMyActorBoundFeature::HandleStateChanged)
        .Register();
}

void UMyActorBoundFeature::HandleStateChanged(bool bIsValid, UObject* SourceObject) 
{ 
    if (!bIsValid) 
    { 
        return; 
    }

    APawn* Pawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn, SourceObject);
    
    // Use pawn resolved relative to SourceObject...
}
```

### 8. Using local dependencies on the client with `nullptr`

If you are on a client and want the local player dependencies, you can pass `nullptr` as the `SourceObject`.

This is especially helpful for UI.

```cpp 
void UMyHUDViewModel::Initialize() 
{ 
    ListenerHandle = MakeSignalRequest(this)
        .AddTag(DependencySignalTags::Controller_Pawn)
        .Bind(this, &UMyHUDViewModel::HandleLocalPawnChanged)
        .Register();
}

void UMyHUDViewModel::HandleLocalPawnChanged(bool bIsValid, UObject* SourceObject) 
{
    if (!bIsValid)
    {
        return;
    }

    APawn* LocalPawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
    // Use LocalPawn for client UI.
}
```

This is a good pattern for:
- HUD setup,
- local widgets,
- input-related UI,
- displaying data for the locally controlled pawn.

### 9. Type-safe dependency traits

To avoid magic tags and repeated casts, declare traits.

**Declaration:**

```cpp 
DECLARE_DEPENDENCY_SIGNAL_TRAIT(FDTMyBossCharacter, Tags::Character, ABossCharacter*);
```
**Usage with `FDependencySignalAgent`:**

```cpp
ABossCharacter* BossCharacter = GetDependency<FDTMyBossCharacter>(this);
```

**Usage with `UDependencySignalSubsystem`:**

```cpp 
if (const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this)) 
{ 
    ABossCharacter* BossCharacter = Subsystem->GetDependency<FDTMyBossCharacter>(); 
}
```

---

## Extending Dependency Resolution with `UDependencySignalProviderBase`

If built-in providers are not enough, you can add your own provider by inheriting from `UDependencySignalProviderBase`.

A provider has two responsibilities:

- `Evaluate(Tag)`  
  Returns whether this provider can handle a given tag.

- `Provide(Tag, SourceObject)`  
  Returns the actual object for that tag and context.

### When to create a custom provider
Create one when you want to resolve custom dependency tags such as:
- inventory owner,
- ability system component,
- equipment manager,
- team state,
- camera manager,
- custom controller-owned components,
- UI-facing objects derived from local player context.

### Minimal example


```cpp 
UCLASS() 
class UDependencySignalProvider_MyGameplay : public UDependencySignalProviderBase 
{ 
    GENERATED_BODY()

protected: 
    virtual bool Evaluate_Implementation(const FGameplayTag& Tag) const override 
    { 
        return Tag.MatchesTag(MyGameplayTags::Inventory); 
    }

    virtual UObject* Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const override
    {
        AController* Controller = GetControllerFrom(SourceObject);
        if (!IsValid(Controller))
        {
            return nullptr;
        }
    
        APawn* Pawn = Controller->GetPawn();
        if (!IsValid(Pawn))
        {
            return nullptr;
        }
    
        return Pawn->FindComponentByClass<UMyInventoryComponent>();
    }
};
```

### Registration
After creating the provider, add it to:

**Project Settings > Game > Dependency Signal Settings > Provider Classes**

Then the subsystem will include it in dependency resolution.

### Why providers are useful
Providers let you keep object lookup rules in one place, instead of scattering custom resolution logic throughout UI, components, controllers, and gameplay systems.

---

## Configuration

Configure the plugin in:

**Project Settings > Game > Dependency Signal Settings**

### Available settings

- **Should Subsystem Be Created**
  - Enables automatic subsystem creation for supported game worlds.

- **Root Tags**
  - Define the root of your dependency hierarchy.
  - By default, the runtime includes `DependencySignal` as the root tag.
  - All dependency tags should inherit from the configured root hierarchy.

- **Provider Classes**
  - A list of `UDependencySignalProviderBase` classes used to resolve dependency objects.

---

## Practical Tips

- Use **specific source objects** when the dependency belongs to a concrete actor/controller flow.
- Use **`nullptr`** when you want **local client dependencies**.
- For UI, `nullptr` is often the cleanest choice.
- Always unregister listeners when your object no longer needs updates.
- Prefer traits in C++ if the same tags are used in many places.
- Keep tags organized hierarchically so your initialization rules stay easy to reason about.

---

## Summary

**Gameplay Dependency Signal** gives you a structured way to say:

- what your system depends on,
- in which context it depends on it,
- when that dependency becomes ready,
- and how to resolve the actual object once it is ready.

It is a strong fit for:
- multiplayer gameplay,
- client UI,
- possession-dependent systems,
- replicated readiness flows,
- decoupled gameplay architecture.