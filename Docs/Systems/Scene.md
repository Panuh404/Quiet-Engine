# Scene and Actors

This chapter explains Scene storage, Actor identity, component access,
handle lifetime, copying, duplication, and runtime lifecycle.
Detailed deferred Actor request contracts and serialization remain planned.

## Purpose and concepts

`Quiet::Scene::Scene` owns the entities and components that describe a scene.

`Quiet::Scene::Actor` provides access to one entity in the Scene's EnTT registry.
An Actor does not independently own that entity or its component storage.

Three identifiers or objects have distinct responsibilities:

| Item | Responsibility |
| --- | --- |
| Quiet UUID | Identifies an Actor for lookup and persistent references |
| EnTT entity identifier | Identifies an entity within a particular registry |
| Actor handle | Connects component operations to that registry and entity |

Keep the Scene context alongside a UUID. A UUID does not itself identify which
Scene should be queried.

## Implementation map

| Source | Principal symbol or responsibility |
| --- | --- |
| `Engine/src/Scene/Scene.h` | `Quiet::Scene::Scene` declarations and owned state |
| `Engine/src/Scene/Scene.cpp` | Creation, lookup, destruction, and scene operations |
| `Engine/src/Scene/Actor.h` | `Quiet::Scene::Actor` and component access templates |
| `Engine/src/Scene/Components/ActorIdentityComponent.h` | UUID and Actor name |
| `Engine/src/Scene/Components/TransformComponent.h` | Actor transform component |
| `Engine/src/Scene/Transform.h` | Position, rotation in degrees, scale, and matrix construction |
| `Engine/src/Editor/EditorRuntimeController.cpp` | Editor runtime Scene creation, transitions, and destruction |
| `Engine/src/Core/Application.cpp` | Frame input/update calls and application shutdown |
| `Engine/src/Physics/PhysicsWorld.cpp` | Physics bodies, synchronization, and fixed stepping |
| `Engine/src/Audio/AudioWorld.cpp` | Runtime sources, listener updates, and audio mix handling |
| `Engine/src/Scripting/ScriptWorld.cpp` | Script instance construction, callbacks, updates, and teardown |

## Ownership and storage

Scene owns an EnTT registry and a separate `std::vector<Actor>`.

The registry stores components. The vector stores Actor handles referring to
entities in that registry.

Scene also owns hierarchy metadata, scene settings, and runtime state.
Hierarchy folders organize editor presentation; they do not create transform
parenting.

Actor is non-copyable and movable. Its move operations are defaulted; moving
an Actor does not explicitly clear the source handle's registry pointer or
entity identifier.

Use Scene operations to create and destroy Actors. Structurally modifying the
vector returned by mutable `GetActors()` can bypass registry and hierarchy
maintenance.

## Actor creation and identity

`Scene::CreateActor()` generates a UUID and delegates to
`Scene::CreateActorWithID()`.

Creation performs these operations:

1. Creates an EnTT entity.
2. Adds `ActorIdentityComponent` with the supplied UUID and name.
3. Adds `TransformComponent`.
4. Appends an Actor handle to the vector.
5. Adds the Actor to the hierarchy root.
6. Returns a reference to the appended handle.

`CreateActorWithID()` does not validate that the supplied UUID is valid or
unused. Callers must preserve those identity invariants.

`Actor::GetID()` reads the identity component. `Actor::GetName()` exposes its
name, with mutable and const overloads.

`Scene::FindActorByID()` searches the Actor vector and returns `nullptr` when
no match exists. `Scene::FindEntityByID()` searches identity components and
returns `entt::null` when no match exists.

## Component access contracts

Scene-created Actors have mandatory identity and Transform components.

Identity is accessed through `GetID()` and `GetName()`; Actor's generic
component templates prohibit `ActorIdentityComponent`.

Transform can be queried and edited through those templates, but cannot be
added or removed.

| Operation | Contract |
| --- | --- |
| `HasComponent<T>()` | Tests component presence on a valid Actor |
| `GetComponent<T>()` | Requires the component to exist; returns a reference |
| `TryGetComponent<T>()` | Returns a pointer, or `nullptr` when the component is absent |
| `AddComponent<T>()` | Requires an optional component that is not already present |
| `RemoveComponent<T>()` | Removes an optional component; returns whether removal occurred |

All component operations require a live registry and valid entity.
`TryGetComponent()` tolerates a missing component; it does not make an expired
Actor safe to access.

The implementation uses assertions for registry/entity validity and duplicate
addition. These assertions do not provide recoverable error results.

## Handle and reference lifetime

Actor pointers and references returned by Scene refer to elements of its
Actor vector.

- Creation can reallocate the vector and invalidate all existing Actor
  pointers and references.
- Destruction erases an element, invalidating references and pointers to that
  element and subsequent elements.
- Clearing, replacing, or destroying a Scene invalidates its previous handles.

For operations spanning structural changes, retain the UUID and look up the
Actor again before accessing it.

Component references refer to registry storage rather than Actor vector
storage. Their lifetime must be considered separately. Do not treat either
kind of reference as a permanent handle.

## Scene copying and Actor duplication

Scene copying and Actor duplication operate at different scopes.

| Behavior | Scene copy | Actor duplication |
| --- | --- | --- |
| Destination | Separate Scene registry | Existing Scene registry |
| Actor UUID | Preserved | Newly generated |
| Actor name | Preserved | Source name followed by ` Copy` |
| Transform | Copied | Copied |
| Supported optional components | Copied when present | Copied when present |
| Hierarchy | Entire hierarchy copied | New Actor appended to hierarchy root |
| Scene settings | Copied | Existing settings remain |
| Runtime worlds | Not copied; destination runtime is inactive | Duplication itself does not create runtime backend objects |

An empty source name becomes `Actor Copy` during Actor duplication.

### Scene copy flow

The copy constructor delegates to copy assignment after constructing an
empty Scene.

`Quiet::Scene::Scene::operator=()` performs these operations:

1. Returns immediately for self-assignment.
2. Clears the destination through `Clear()`.
3. Copies audio, physics, and render settings.
4. Recreates each source Actor with its existing UUID and name.
5. Copies its Transform and supported optional components.
6. Copies the source hierarchy.
7. Leaves the destination runtime mode as `RuntimeMode::None`.

The destination owns separate entity and component storage. Its Actor
handles refer to its own registry.

Source Actor pointers and EnTT entity identifiers must not be used to
access the destination. Resolve destination Actors by UUID through the
destination Scene.

Assignment invalidates the destination's previous Actor handles and
component references. It replaces the destination in place; there is no
rollback path if component copying or allocation throws.

`Engine/src/Editor/EditorRuntimeController.cpp` uses this copy constructor
in `Quiet::Editor::EditorRuntimeController::CreateRuntimeScene()`, then
starts the copied Scene's runtime separately.

### Supported component copies

Both Scene copying and Actor duplication explicitly copy these optional
components when present:

- `StaticMeshComponent`
- `MaterialComponent`
- `DirectionalLightComponent`
- `RotatorComponent`
- `CameraComponent`
- `AudioSourceComponent`
- `AudioListenerComponent`
- `ScriptComponent`
- `RigidBodyComponent`
- `BoxColliderComponent`
- `SphereColliderComponent`
- `CapsuleColliderComponent`

These lists are implemented in `Engine/src/Scene/Scene.cpp`, inside
`Quiet::Scene::Scene::operator=()` and
`Quiet::Scene::Scene::TryDuplicateActorWithID()`.

Copying is not automatically extended to every component stored in EnTT.
Adding another component type requires reviewing both explicit copy paths.

### Separate component storage and shared resources

Copying `StaticMeshComponent` copies its `MeshResourceID`, material slots,
visibility, and shared mesh pointer.

The component objects and material-slot vectors are separate copies.
The `Mesh` pointers retain shared ownership of the same loaded
`Quiet::Renderer::Vulkan::StaticMesh`.

Consequently, changing a copied Actor's Transform changes that Actor's
component data. Copying the Scene does not create an independent copy of
every loaded rendering resource.

The relevant declaration is
`Engine/src/Scene/Components/StaticMeshComponent.h`.

### Script Actor references

`ScriptComponent` and its property vector are copied by value.
`ScriptProperty::ActorValue` UUIDs are preserved without remapping.

A Scene copy preserves all Actor UUIDs, allowing those values to identify
corresponding Actors when resolved against the copied Scene.

Duplicating one Actor creates a new UUID but leaves its script properties
unchanged. A property referring to the source Actor therefore continues
to refer to the source; it is not automatically redirected to the duplicate.

Both declarations are in
`Engine/src/Scene/Components/ScriptComponent.h`.

### Actor duplication flow

`TryDuplicateActor()` generates a UUID and delegates to the private
`TryDuplicateActorWithID()` implementation.

That implementation:

1. Rejects an invalid or already-used destination UUID.
2. Finds the source Actor, returning `nullptr` if it is absent.
3. Copies the name, Transform, and supported optional components into
   local values.
4. Creates the duplicate with the new UUID and derived name.
5. Applies the saved component values.
6. Returns a pointer to the new Scene-owned Actor handle.

Saving component values before creating the duplicate matters because
creation may reallocate the Actor vector and invalidate the source pointer.

The duplicate is appended to the hierarchy root. It does not inherit the
source Actor's folder placement.

`DuplicateActor()` wraps the pointer-returning operation. If that operation
returns `nullptr`, the wrapper logs an error and throws `std::runtime_error`.

The `Try` variant reports its explicit rejection cases through `nullptr`;
it does not guarantee that allocation or component copying cannot throw.

Both immediate duplication APIs can invalidate existing Actor pointers and
references through vector reallocation. Retain UUIDs and reacquire handles
after structural changes.

## Runtime lifecycle

Runtime connects a Scene's component data to physics, audio, and scripting
worlds. These worlds belong to the Scene; application-level dependencies
are owned elsewhere.

### Runtime modes

`Quiet::Scene::RuntimeMode` describes the Scene's runtime state.

| Mode | Scene behavior |
| --- | --- |
| `None` | Runtime input and update calls return without performing runtime work |
| `Simulate` | Physics, audio, and scripting run; the Scene's Simulate input handler is currently empty |
| `Play` | Physics, audio, and scripting run; Scene input updates the primary camera when one exists |

Simulate runs scripting and audio as well as physics.

The Editor's Edit, Simulate, and Play modes are managed separately by the
editor controller. Scene runtime mode does not itself create a copied Scene,
switch editor UI state, or control mouse capture.

### Ownership and borrowed dependencies

Scene owns its `PhysicsWorld`, `AudioWorld`, and `ScriptWorld`.

- PhysicsWorld borrows the application-owned PhysicsSubsystem.
- AudioWorld borrows the application-owned AudioSubsystem.
- ScriptWorld borrows its Scene, ProjectContext, and Input.

These dependencies must remain alive while the worlds use them.

`Application::ShutdownCommon()` ends active runtime before destroying its
Scenes and application-level subsystems.

### Runtime startup

`Scene::BeginRuntime()` performs these operations:

1. Rejects `RuntimeMode::None` with a warning and returns without changing
   the current runtime.
2. Ends an existing active runtime before starting the requested mode.
3. Clears contact events, creates PhysicsWorld, and builds physics bodies
   from the registry.
4. Creates AudioWorld, applies scene mix settings, resolves the primary
   audio listener, and builds audio sources.
5. Creates ScriptWorld using the Scene, ProjectContext, and Input.
6. Clears pending Actor creation and destruction requests.
7. Sets the requested runtime mode and invokes its Scene begin hook.
8. Builds script instances and begins them.

Script `OnCreate` callbacks therefore run after native world construction
and after the Scene is marked runtime-active.

AudioWorld establishes its listener before starting eligible
`PlayOnStart` sources.

The Scene's Simulate and Play begin/end hook bodies are currently empty.
Their dispatch functions select the hook and log the transition.

`BeginRuntime()` returns no success value. Startup exceptions are not caught
by this function, and it has no rollback path for partially constructed
runtime state.

`IsRuntimeActive()` tests the mode flag. It does not confirm that every
script loaded or every requested backend object was created successfully.

### Input and frame updates

Application frame code calls `HandleRuntimeInput()` before `Update()` for
active runtime Scenes. Both methods return immediately while runtime is
inactive.

In Play, Scene input resolves the primary camera and updates its Transform
through `CameraController::UpdateFromInput()` and the camera pilot helpers.
Without a primary camera, that handler returns.

Simulate viewport navigation is handled by Application's editor camera
logic; the Scene's Simulate input handler currently performs no work.

`deltaTime` is the frame duration in seconds. Scene does not perform a
common validation or clamp before passing it through its runtime phases.

`Scene::UpdateRuntime()` executes these phases in order:

| Phase | Operation |
| --- | --- |
| 1 | Update eligible Rotator components |
| 2 | Synchronize/reload script instances and run script updates |
| 3 | Update physics and collect contact events |
| 4 | Dispatch contact events to scripts |
| 5 | Flush deferred Actor creation requests |
| 6 | Flush deferred Actor destruction requests |
| 7 | Synchronize physics bodies after structural changes |
| 8 | Resolve the primary audio listener and update runtime audio |

Rotators update Actors without RigidBodyComponent and Actors with kinematic
rigid bodies. Actors with other rigid-body motion types are skipped.

PhysicsWorld also synchronizes bodies at the beginning of its update.
Its simulation uses a fixed step of `1 / 60` seconds, with accumulated time
capped at four steps. Time beyond that cap is discarded.

Script updates receive the frame duration rather than one callback for each
physics step.

Runtime updates can create or destroy Actors while flushing requests.
Actor pointers and references must therefore not be retained across updates
as permanent handles. Detailed request acceptance and cancellation contracts
will be documented in the next increment.

### Runtime shutdown

`Scene::EndRuntime()` returns immediately when runtime mode is `None`.
For an active runtime, it:

1. Ends scripts.
2. Invokes the Scene's mode-end hook.
3. Releases ScriptWorld, AudioWorld, and PhysicsWorld, in that order.
4. Clears pending Actor requests and contact events.
5. Sets runtime mode to `None`.

Script teardown runs before native worlds are released.
ScriptWorld ends its instances in reverse creation order, invoking
`OnDestroy` for started instances that provide that callback.

AudioWorld destroys its sources and restores the previously saved
application audio mix when applicable.

Ending runtime does not clear the Actor registry, hierarchy, or scene
settings, and does not restore component values to their pre-runtime state.

The Editor separately destroys its runtime Scene copy after ending runtime.
Discarding that copy leaves the authored editor Scene available.

### EndRuntime and Clear

`EndRuntime()` stops runtime while retaining scene data.

`Clear()` resets runtime worlds, Actor storage, hierarchy, settings, queues,
and contact events. It does not call `Scene::EndRuntime()` or dispatch the
Scene mode-end hook.

Resetting ScriptWorld still invokes its destructor, whose shutdown path
ends scripts. Clear therefore must not be described as skipping all script
teardown.

Copy assignment begins with `Clear()`. It does not use the same Scene
mode-end dispatch as an explicit `EndRuntime()` call.

## Worked examples

### Creation and lookup

The following statements are illustrative code inside an initialized engine
context with an existing editable `Quiet::Scene::Scene& scene`.
They are not a standalone program.

```cpp
const auto firstID = scene.CreateActor("First").GetID();
const auto secondID = scene.CreateActor("Second").GetID();

// The second creation may have moved the Actor handles.
// Resolve First again instead of retaining its creation reference.
if (auto* first = scene.FindActorByID(firstID))
{
    auto& transform =
        first->GetComponent<Quiet::Scene::TransformComponent>();

    transform.WorldTransform.Position = {1.0f, 0.0f, 0.0f};
}

scene.DestroyActorByID(secondID);
```

The component reference is used within the lookup scope, before another
structural operation.

### Scene copying and Actor duplication

These statements are illustrative code inside an initialized engine context
with an existing editable `Quiet::Scene::Scene& scene`. They use the Scene and
Transform declarations introduced earlier.

```cpp
const auto sourceID = scene.CreateActor("Source").GetID();

// The copied Scene preserves sourceID in a separate registry.
Quiet::Scene::Scene copiedScene(scene);

if (auto* copiedActor = copiedScene.FindActorByID(sourceID))
{
    copiedActor
        ->GetComponent<Quiet::Scene::TransformComponent>()
        .WorldTransform.Position.x = 3.0f;
}

// Duplication creates another Actor in the original Scene.
if (auto* duplicate = scene.TryDuplicateActor(sourceID))
{
    duplicate
        ->GetComponent<Quiet::Scene::TransformComponent>()
        .WorldTransform.Position.x = 6.0f;
}
```

Expected results:

- The original `Source` Actor retains its initial Transform.
- The corresponding Actor in `copiedScene` retains `sourceID` and has X = 3.
- The duplicate in `scene` has a new UUID, the name `Source Copy`, and X = 6.
- `copiedScene` remains runtime-inactive.

The example uses each returned handle before another structural operation.
It does not retain the source Actor pointer across duplication.

### Entering and stopping Play in the Editor

This example traces the existing editor workflow rather than introducing
another runtime controller.

The entry point is
`Quiet::Editor::EditorRuntimeController::EnterPlayMode()` in
`Engine/src/Editor/EditorRuntimeController.cpp`.

1. The controller checks that the Editor is in Edit mode.
2. It clears the piloted-camera selection.
3. `CreateRuntimeScene()` destroys any previous runtime Scene and stops
   editor audio preview.
4. It copy-constructs a Scene from the authored editor Scene.
5. It calls `BeginRuntime(RuntimeMode::Play, ...)` on that copy.
6. After startup returns, it sets the Editor mode to Play, saves the previous
   mouse-capture state, and enables capture.
7. Application frame code applies runtime input and updates the active copy.
8. `StopPlayMode()` ends runtime and destroys the copy, returns the Editor
   to Edit mode, and restores the previous mouse-capture state.

The authored Scene remains separate from runtime component changes.
Stopping the runtime copy does not copy its values back into the authored Scene.

## Current limitations and extension points

Actor lookup currently searches the handle vector linearly.

Explicit-ID creation relies on the caller to supply a valid, unused UUID.
Validation would belong in `Scene::CreateActorWithID()` if this policy changes.

Direct component access modifies registry data. These templates do not
themselves record editor history or explicitly synchronize runtime worlds.
Those integrations require separate documentation.

The remaining increments will cover detailed deferred Actor request
contracts and serialization.