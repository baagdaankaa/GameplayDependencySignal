# Gameplay Dependency Signal

**Gameplay Dependency Signal** is a lightweight, event-driven dependency system for Unreal Engine. Gameplay code, UI and other runtime systems use it to wait for objects and states.

Instead of checking whether a `Controller`, `Pawn`, `PlayerState` or custom dependency is ready, you register interest in one or more **gameplay tags** and get a callback when the dependency becomes valid or invalid. This helps most in multiplayer, where spawning order, possession, replication and ownership changes delay when objects become available.

---

## Table of Contents

- [Why Use It?](#why-use-it)
- [Core Concepts](#core-concepts)
  - [Tags and Hierarchy](#tags-and-hierarchy)
  - [`SourceObject`](#sourceobject)
  - [Validation vs Resolution](#validation-vs-resolution)
- [Blueprints](#blueprints)
- [C++](#c)
  - [Agent vs Subsystem](#agent-vs-subsystem)
  - [Examples](#examples)
- [Custom Providers](#custom-providers)
- [Configuration](#configuration)
- [Debugging](#debugging)
- [Practical Tips](#practical-tips)

---

## Why Use It?

Gameplay systems rarely become ready at the same time. A widget needs the local controller and pawn, a component needs a possessed pawn, and a feature needs `PlayerState` plus a custom tag. Handling this with `BeginPlay`, `Tick`, timers or chains of null checks gets noisy and fragile.

With Dependency Signal you:
- **register** a request for one or more tags,
- **listen** for the combined validity to change (valid **and** invalid transitions),
- **resolve** the dependency object once it is ready.

Key features:
- One `UDependencySignalSubsystem` per game world manages listeners, validity state and providers.
- Dependencies are organized with hierarchical gameplay tags.
- The same tag is tracked separately per `SourceObject`, including a local client context.
- Providers resolve tags to objects; controller, pawn and player state are supported out of the box.
- Full Blueprint support: an async listen node and library functions.
- Type-safe C++ access through dependency traits.
- A GameplayDebugger category shows server and client dispatcher state at runtime.

---

## Core Concepts

> A dependency is identified by a **gameplay tag** and optionally scoped by a **`SourceObject`**.

### Tags and Hierarchy

Built-in tags:

```
DependencySignal
└── DependencySignal.Controller
    ├── DependencySignal.Controller.Pawn
    └── DependencySignal.Controller.PlayerState
```

A child tag can only be effectively valid when its whole parent chain is valid. If `DependencySignal.Controller` is invalid, `DependencySignal.Controller.Pawn` is invalid too, whatever it was validated as. This lets you express initialization order with the tag tree instead of scattered checks.

### `SourceObject`

`SourceObject` is the **scope** a dependency state belongs to. `DependencySignal.Controller.Pawn` for `PlayerController A`, for `PlayerController B` and for the local client are three independent entries.

- **Real object** (controller, pawn, owned actor or component): the dependency belongs to that specific object.
- **`nullptr`**: the **local session**, i.e. the local world and player context. This is the cleanest choice for client-only code such as the HUD, widgets and menus, which need "my pawn" rather than one particular replicated object.

### Validation vs Resolution

These are related but separate steps:
- **Validation** marks whether a dependency is ready (`Validate(Tag, bIsValid, SourceObject)`).
- **Resolution** obtains the actual object for a tag through a provider (`GetDependency(Tag, SourceObject)`).

The built-in controller provider resolves `Controller`, `Controller.Pawn` and `Controller.PlayerState`. It finds the controller through the source's actor/owner chain and, in local scenarios, falls back to the first local player controller.

---

## Blueprints

**Wait for dependencies:** the async node broadcasts whenever the combined validity of its tags changes.

<img width="465" height="256" alt="Listen For Dependency Signal Validity Node" src="https://github.com/user-attachments/assets/e9068235-0de8-44c3-ad2a-531d08ba7e04" />

**Validate / invalidate:**

<img width="959" height="286" alt="Validation Dependency Nodes" src="https://github.com/user-attachments/assets/176adacf-ca50-45f5-8b49-bc6cd639786b" />

**Check validity immediately** (no subscription):

<img width="767" height="212" alt="Is Valid Dependency Nodes" src="https://github.com/user-attachments/assets/84f6b483-52be-4975-a6fa-c75232983033" />

**Get the object:** for local UI, `Get Local Dependency Signal Object` uses the local context directly.

<img width="732" height="217" alt="Get Dependency Object Nodes" src="https://github.com/user-attachments/assets/2649cc9b-0c20-4fb6-bb9d-e08d80b966bf" />

**Manage a stored listener handle**, e.g. to stop listening manually:

<img width="756" height="152" alt="Listener Handle Nodes" src="https://github.com/user-attachments/assets/47424483-666d-40e5-8347-aca1c72294a7" />

---

## C++

### Agent vs Subsystem

| | `FDependencySignalAgent` | `UDependencySignalSubsystem` |
|---|---|---|
| **How** | Inherit it in your class | Call `UDependencySignalSubsystem::Get(this)` |
| **Gives you** | Fluent `MakeSignalRequest(...)`, `ValidateDependency(...)`, `GetDependencyObject(...)`, cached subsystem lookup | The explicit runtime API: `Register`, `Unregister`, `Validate`, `IsValidDependency`, `GetDependency` |
| **Best for** | Most gameplay classes | Framework, utility and centralized service code |

### Examples

#### 1. Listen with the agent (local context)

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
        .AddSource(nullptr) // local session; also the default when AddSource is omitted
        .Bind(this, &UMyWidgetController::HandlePawnStateChanged)
        .Register();
}

void UMyWidgetController::Shutdown()
{
    PawnListenerHandle.Unregister();
}

void UMyWidgetController::HandlePawnStateChanged(bool bIsValid, UObject* SourceObject)
{
    if (bIsValid)
    {
        APawn* LocalPawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
        // Initialize UI with LocalPawn...
    }
}
```

#### 2. Multiple tags, explicit source

The callback reports the **combined** state: it is valid only when every tag is valid. Tags can come from different levels of the same branch.

```cpp
ListenerHandle = MakeSignalRequest(this)
    .AddTag(DependencySignalTags::Controller)
    .AddTag(DependencySignalTags::Controller_Pawn)
    .AddTag(DependencySignalTags::Controller_PlayerState)
    .AddSource(GetOwningPlayer()) // state scoped to this controller
    .Bind(this, &UMyFeature::HandleDependenciesChanged)
    .Register();

void UMyFeature::HandleDependenciesChanged(bool bIsValid, UObject* SourceObject)
{
    if (!bIsValid)
    {
        return; // at least one dependency is missing or became invalid
    }

    APawn* Pawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn, SourceObject);
    APlayerState* PlayerState =
        GetDependencyObject<APlayerState>(this, DependencySignalTags::Controller_PlayerState, SourceObject);
    // Safe initialization path...
}
```

#### 3. Lambda callback

`Bind` with an owner object binds a weak lambda: the listener lives only as long as that object.

```cpp
ListenerHandle = MakeSignalRequest(this)
    .AddTag(DependencySignalTags::Controller_Pawn)
    .Bind(this, [this](bool bIsValid, UObject* SourceObject)
    {
        if (bIsValid)
        {
            APawn* LocalPawn = GetDependencyObject<APawn>(this, DependencySignalTags::Controller_Pawn);
            // React to LocalPawn...
        }
    })
    .Register();
```

#### 4. Directly through the subsystem

```cpp
void UMyManager::StartListening()
{
    if (UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this))
    {
        FDependencySignalRequest Request;
        Request.AddTag(DependencySignalTags::Controller)
            .AddTag(DependencySignalTags::Controller_Pawn)
            .BindUObject(this, &UMyManager::HandleDependencyChanged);

        ListenerHandle = Subsystem->Register(Request.End());
    }
}

void UMyManager::HandleDependencyChanged(bool bIsValid, UObject* SourceObject)
{
    if (const UDependencySignalSubsystem* Subsystem = UDependencySignalSubsystem::Get(this); Subsystem && bIsValid)
    {
        APawn* Pawn = Subsystem->GetDependency<APawn>(DependencySignalTags::Controller_Pawn);
        // Use Pawn...
    }
}
```

Unregister with `Subsystem->Unregister(ListenerHandle)` or `ListenerHandle.Unregister()`.

#### 5. Validating a dependency

Validate when an object becomes ready, and invalidate when it goes away:

```cpp
void AMyPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ValidateDependency(this, DependencySignalTags::Controller_Pawn, IsValid(InPawn));
}

void AMyPlayerController::OnUnPossess()
{
    ValidateDependency(this, DependencySignalTags::Controller_Pawn, false);
    Super::OnUnPossess();
}
```

Pass `bForceRetrigger = true` to notify listeners even if the state did not change.

#### 6. Type-safe traits

Map a tag to a C++ type once to avoid magic tags and casts:

```cpp
DECLARE_DEPENDENCY_SIGNAL_TRAIT(FDTMyBossCharacter, Tags::Character, ABossCharacter*);

ABossCharacter* BossFromAgent = GetDependency<FDTMyBossCharacter>(this);
ABossCharacter* BossFromSubsystem = Subsystem->GetDependency<FDTMyBossCharacter>();
```

---

## Custom Providers

When the built-in provider is not enough, inherit from `UDependencySignalProviderBase`. It needs two functions:
- `Evaluate(Tag)`: can this provider handle the tag?
- `Provide(Tag, SourceObject)`: return the object for the tag in that context.

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
        const AController* Controller = GetControllerFrom(SourceObject);
        const APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
        return IsValid(Pawn) ? Pawn->FindComponentByClass<UMyInventoryComponent>() : nullptr;
    }
};
```

Register it in **Project Settings > Game > Dependency Signal Settings > Provider Classes**. Providers keep lookup rules in one place instead of scattering them across UI, components and controllers. Typical uses: inventory, ability system component, equipment, team state, or camera manager.

---

## Configuration

**Project Settings > Game > Dependency Signal Settings**

- **Should Subsystem Be Created**: enables the subsystem for game worlds.
- **Root Tags**: roots of the dependency hierarchy (`DependencySignal` by default). Root tags are always valid; all dependency tags should live under them.
- **Provider Classes**: the `UDependencySignalProviderBase` classes used for resolution.

---

## Debugging

The plugin registers a **`DependencySignal`** category in the [GameplayDebugger](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-gameplay-debugger-in-unreal-engine). It is compiled only in builds with the GameplayDebugger (stripped from Shipping) and is enabled by default.

Open the GameplayDebugger with **`'`** (apostrophe). The category lists every registered source and its tags:

- **Green**: valid. **Red**: invalid. **Orange**: valid itself, but a parent tag is invalid.
- Each tag shows its listener count; the header shows total listeners and any-source listeners.
- Sources are ordered: the selected **debug actor** (and its components, marked `>`), then **`<Local Session>`** (the `nullptr` context, always listed), then alphabetically.

**Server and client state.** The data is collected on the **authority** and replicated by the GameplayDebugger, so by default a client sees the **server's** dispatcher. Each world has its own dispatcher, so the server and client states can differ.

| Key | Action |
|---|---|
| `Shift+V` | Switch between server and client (local) state. Disabled in standalone, where both are the same. |
| `Shift+N` / `Shift+B` | Next / previous page |

When the list is taller than the screen it continues in more columns; when the columns are wider than the screen they are split into pages. All keys can be rebound in the GameplayDebugger input settings (`ToggleServerClientState`, `NextPage`, `PreviousPage`).

---

## Practical Tips

- Use a **real `SourceObject`** for dependencies of a concrete actor or controller; use **`nullptr`** for local client code, especially UI.
- Always validate **and** invalidate: listeners react to both transitions.
- Always unregister listeners when the owner no longer needs updates.
- Prefer traits in C++ for tags used in many places.
- Keep tags hierarchical so initialization rules follow the tag tree.
- When a client and the server disagree, use `Shift+V` in the GameplayDebugger to compare both states.
