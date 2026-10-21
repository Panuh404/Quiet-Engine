# Scene and Actors <!--! {#quiet_system_scene} -->

This chapter explains Scene storage, Actor identity, component access,
handle lifetime, copying, duplication, runtime lifecycle, deferred Actor
requests, and serialization.

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
| `Engine/src/Scripting/Bindings/SceneBindings.cpp` | Lua Scene creation and lookup bindings |
| `Engine/src/Scripting/Bindings/ActorBindings.cpp` | UUID-based Lua Actor references, duplication, and destruction |
| `Engine/src/Scene/SceneSerializer.h` | `Quiet::Scene::SceneSerializer` file persistence declarations |
| `Engine/src/Scene/SceneSerializer.cpp` | JSON construction, loading, and mesh resolution |
| `Engine/include/Quiet/Scene/SceneDocument.h` | `Quiet::Scene::SceneDocument` document operations |
| `Engine/src/Scene/SceneDocument.cpp` | Persistence forwarding and default Scene construction |
| `Engine/src/Renderer/Vulkan/Mesh/MeshLibrary.h` | Lookup of already-loaded mesh resources |
| `Engine/src/Editor/EditorSceneWorkflow.cpp` | Editor load/save requests and session updates |
| `Engine/src/Project/ProjectWorkflow.cpp` | Editable startup Scene loading and fallback |

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

## Authored text data

TextComponent owns UTF-8 content, pixel height, linear RGBA color, and
visibility. It contains no backend resource or execution state.

Scene copying, Actor duplication, Scene persistence, and Editor history
preserve these values. History compares content and visibility exactly
and uses the existing float tolerance for height and color.

The renderer consumes visible text components using its default font.
All text currently shares screen-space origin (24, 24). Inspector supports
addition, removal, multiline content, pixel height, color, and visibility.
Positioning and font selection remain unimplemented.

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
as permanent handles. The deferred request contracts are described below.

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

## Deferred Actor requests

Runtime callbacks use queued Actor operations so structural changes can
be applied after script updates and physics contact callbacks.

The request methods are implemented in `Engine/src/Scene/Scene.cpp`.

### Acceptance and materialization

Acceptance records a request and, for creation or duplication, reserves
a UUID in the pending creation queue.

Materialization creates the registry entity, mandatory components, and
Actor handle during the runtime creation phase.

These are separate events. A reserved UUID does not yet identify an
Actor that `FindActorByID()` can return.

| Request | Acceptance result | Immediate effect | Deferred effect |
| --- | --- | --- | --- |
| Create | Reserved UUID, or invalid UUID if inactive | Adds a creation request | Creates an Actor |
| Duplicate | Reserved UUID, or invalid UUID if inactive or source is invalid/absent | Adds a duplication request | Copies the source into a new Actor |
| Destroy existing Actor | True when queued or already queued | Records destruction once | Notifies scripting, then removes the Actor |
| Destroy pending creation | True when canceled | Removes the pending creation request | The Actor is never materialized |

An invalid UUID is the default UUID value whose `IsValid()` returns false.

A UUID-returning request generates an ID that is valid and does not match
an existing Actor or another pending creation request.

The request methods do not report eventual completion. Allocation
exceptions are not caught by these methods.

### Creation requests

`RequestRuntimeActorCreate()` rejects requests while runtime is inactive.

An accepted request records its UUID and name. An empty name becomes
`Actor`, unlike immediate `CreateActor()`, which retains the supplied name.

During `FlushRuntimeActorCreations()`, ordinary creation delegates to
`CreateActorWithID()` and produces the usual identity and Transform
components.

Creation requests do not provide a component-initialization payload.
Configure the Actor after it has materialized.

### Duplication requests

`RequestRuntimeActorDuplicate()` requires an existing source Actor.
A UUID belonging only to another pending creation cannot be duplicated yet.

Acceptance records the source UUID and reserved destination UUID.
It does not capture the source's component values.

During materialization, `TryDuplicateActorWithID()` reads the source's
current name and supported components.

Source data can change between acceptance and materialization. For example,
physics runs before the creation phase and may update the source Transform.

If the source is absent when the creation batch is processed, duplication
is skipped. The reserved destination UUID does not become an Actor.

A source queued for deferred destruction still exists during the creation
phase. Its queued duplication can therefore materialize before that source
is removed.

### Destruction and cancellation

`RequestRuntimeActorDestroy()` rejects inactive runtime, invalid UUIDs,
and UUIDs that identify neither an existing Actor nor a pending creation.

For an existing Actor, the UUID is added to the destruction queue once.
Repeated requests return true without adding another entry.

The Actor remains findable until the destruction phase. Acceptance does
not immediately erase its components or make its Lua reference invalid.

For an Actor that has not materialized, the method searches the pending
creation queue and removes the matching request immediately.
This also supports canceling a pending duplicate.

Canceled creation does not construct an entity or script instance and
therefore has no created instance to receive lifecycle callbacks.

### Flush ordering and callbacks

`Scene::UpdateRuntime()` flushes creations before destructions, after
script updates, physics, and contact callbacks.

The creation phase moves the pending creation queue into a local batch.
Requests added later are left for a subsequent creation phase.

The destruction phase walks its queue by index. Requests appended by an
`OnDestroy` callback for another existing Actor can be processed later
in that same destruction phase.

For each existing Actor being destroyed:

1. Scene calls `ScriptWorld::NotifyActorWillBeDestroyed()`.
2. ScriptWorld removes the matching instance, invoking `OnDestroy` when
   the instance started and supplies that callback.
3. Scene calls `DestroyActorByID()`, removing the entity, Actor handle,
   and hierarchy references.

Script teardown occurs while the Actor still exists and the native worlds
remain alive.

Creation requested from an `OnDestroy` callback waits for a later creation
phase because the current creation phase has already finished.

After both flushes, physics reconciles its bodies and audio reconciles its
sources during the remaining runtime update.

Eligible scripts on newly materialized Actors are discovered during a
subsequent ScriptWorld update. Successful instances begin then, rather
than during the creation flush itself.

### Lua Actor references

The Lua bindings wrap a Scene reference and UUID, resolving the Actor when
an operation needs it. They do not retain a pointer into the Actor vector.

`Scene.CreateActor()` and an Actor reference's `Duplicate()` can return
a wrapper for an accepted request before the Actor exists.

For such a pending wrapper:

- `GetID()` returns the reserved UUID string.
- `IsValid()` returns false until the Actor materializes.
- `Destroy()` can cancel its pending creation.
- Name and component access require a materialized Actor.

`Scene.FindActorByID()` returns nil for a pending UUID because its lookup
requires an existing Actor.

After deferred destruction, an existing wrapper's `IsValid()` returns false.
The wrapper retaining a UUID does not keep the entity alive.

The Lua-facing API is documented in the [Scripting guide](../Guides/Scripting.md).

### Immediate operations and queue lifetime

Immediate `CreateActor()`, duplication, and `DestroyActorByID()` are not
blocked merely because runtime is active.

They do not provide the deferred request phases. In particular,
`DestroyActorByID()` does not itself notify ScriptWorld before removing
the entity.

Use the request APIs for structural operations issued from runtime callbacks.

`EndRuntime()` and `Clear()` discard pending creation and destruction
requests. Requests issued during runtime-end script callbacks can therefore
be accepted and subsequently discarded during shutdown.

Acceptance is not a guarantee that an Actor will materialize or that a
queued destruction will execute.

## Scene serialization

Serialization records supported Scene data as JSON. Loading reconstructs
entities and components, resolving mesh references against an existing library.

It does not restore a running physics, audio, or scripting world.

### Document and serializer responsibilities

`Quiet::Scene::SceneDocument` exposes default Scene construction and file
load/save operations.

Its `Save()` and `Load()` methods directly forward to SceneSerializer.
They add no exception handling, rollback, resource importing, or fallback.

`CreateDefault()` clears the destination and constructs a Cube Actor and
primary Camera Actor. It requires the loaded `Cube` mesh resource through
`MeshLibrary::Get()`. If that resource is absent, lookup throws after the
destination has already been cleared.

Fallback decisions belong to callers, not SceneDocument.

### Persistent data

The saved root contains these sections:

| Section | Stored data |
| --- | --- |
| `actors` | Actor UUIDs, names, Transforms, and recognized optional components |
| `audio_settings` | Master, music, SFX, and UI volumes |
| `render_settings` | Scene light and ambient settings |
| `physics_settings` | Gravity |
| `editor_hierarchy` | Root ordering, folders, folder expansion, and membership |

Transforms store position, rotation in degrees, and scale.

Optional component serialization is explicit, like the copy paths described
earlier. Adding a persistent component requires updating both save and load
logic; EnTT storage alone does not make it serializable.

Static meshes are represented by resource IDs, visibility, and material slots.
The loaded mesh object itself is not embedded.

A StaticMeshComponent is written only when its `IsValid()` returns true,
which currently means its mesh pointer is non-null.

Script components store the script asset ID and named property values.
Properties with empty names are omitted. Actor-reference properties store
UUID strings; asset-reference properties store asset IDs.

EnTT entity identifiers, runtime worlds, physics body state, audio playback
state, Lua environments, pending requests, and contact events are not written.

The file contains no explicit schema-version field. The exhaustive field
reference is documented in the [Scene file format](../Reference/SceneFiles.md).

### Save flow and result

`SceneSerializer::SaveToFile()`:

1. Constructs JSON from the current Scene data.
2. Opens the destination file for writing.
3. Returns false if the file cannot be opened.
4. Writes JSON formatted with four-space indentation.
5. Logs the save and returns true.

The function does not create parent directories.
EditorSceneWorkflow ensures project directories exist before calling Save.

Saving opens the destination directly, replacing existing contents.
There is no temporary-file-and-rename operation.

The final stream state is not checked after writing. A true result means
the function reached its success return after issuing the write; it does
not establish that the complete file was successfully written and closed.

JSON construction or serialization exceptions propagate.

The serializer does not enforce Edit mode. Saving an active runtime Scene
records its current component values rather than its pre-runtime state.

Pending creation requests are absent from Actor storage and are not saved.
Actors awaiting deferred destruction remain part of the saved data until
that destruction is applied.

### Load flow

`SceneSerializer::LoadFromFile()`:

1. Opens the file, returning false if it cannot be opened.
2. Parses JSON.
3. Checks that the root contains an `actors` array, returning false otherwise.
4. Clears the destination Scene.
5. Loads available scene settings.
6. Recreates Actors and recognized components.
7. Restores or rebuilds hierarchy metadata.
8. Logs the load and returns true.

Failure to open the file or failure of the explicit `actors` check leaves
the destination unchanged.

Parsing exceptions occur before Clear. Exceptions during reconstruction
occur after Clear and can leave settings and Actors partially rebuilt.
The loader neither catches those exceptions nor restores the original Scene.

Successful reconstruction leaves runtime inactive. Starting runtime is a
separate operation.

### Identity, defaults, and validation

Each Actor requires a string `id` and Transform data containing position,
rotation, and scale. A missing name defaults to `Actor`.

If UUIDFromString reports an Actor ID parse failure, that Actor is logged
and skipped. Other required-field or type failures can throw.

The Actor-loading loop does not separately enforce UUID `IsValid()` or
uniqueness after parsing. It therefore must not be described as validating
all identity invariants.

Absent optional sections and fields use component or scene defaults where
the corresponding load code provides them. This is field-specific handling,
not comprehensive schema validation.

Unsupported script-property types and unnamed properties are skipped.
Script Actor-reference values are restored as UUID data without verifying
that the referenced Actor exists.

### Resource resolution

Loading receives a MeshLibrary containing already-loaded resources.

For each `static_mesh` section, the loader calls `MeshLibrary::TryGet()`
with its resource ID.

When lookup succeeds, the Actor receives StaticMeshComponent and shared
ownership of that mesh. Visibility and material slots are restored.

When lookup fails, the loader logs a warning and omits StaticMeshComponent.
The Actor and its other recognized components can still load.

The serializer does not import assets or populate the mesh library.
Texture, audio, and script references are restored as component data;
the serializer does not instantiate their runtime backends.

Load required meshes before reconstructing a Scene that references them.

### Hierarchy restoration

Absent or unusable hierarchy metadata falls back to
`ResetHierarchyFromActors()`.

Parsed metadata is applied through `SetHierarchy()`, which sanitizes it.
An empty parsed hierarchy also falls back to Actor-based reconstruction
when the Scene contains Actors.

Hierarchy restoration does not establish transform parenting.

### Boolean results and caller policy

A true load result means reconstruction reached its final return.
It does not guarantee that every Actor or resource reference was retained.

| Condition | Result |
| --- | --- |
| File cannot be opened | False; destination unchanged |
| Root lacks an `actors` array | False; destination unchanged |
| JSON parsing throws | Exception; destination not yet cleared |
| Required reconstruction data throws | Exception; destination may be partially rebuilt |
| Actor UUID parser reports failure | Actor skipped; loading continues |
| Mesh lookup fails | Mesh component omitted; loading continues |

EditorSceneWorkflow restricts normal load/save requests to Edit mode.
After successful load, it resets the editor Scene session. After successful
save, it marks the Scene saved.

ProjectWorkflow's editable startup helper creates a default Scene when
SceneDocument::Load returns false. That helper does not catch serializer
exceptions and turn them into the same fallback.

Packaged Sandbox startup treats a false Scene load result as an error.
Its runtime begins separately after loading.

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

### Canceling creation before materialization

These statements are illustrative code using an existing active
`Quiet::Scene::Scene& scene`, before its next runtime update.

```cpp
const auto pendingID = scene.RequestRuntimeActorCreate("Temporary");

if (pendingID.IsValid())
{
    scene.RequestRuntimeActorDestroy(pendingID);
}
```

The creation request reserves an ID but does not immediately create an Actor.
The destruction request finds and cancels that pending creation.

With no update between these statements, `Temporary` never materializes.
No script instance is constructed for it.

### Duplicating and destroying a source in one update

Assume an existing source Actor issues these requests during a runtime
callback, and no other operation removes it immediately:

```cpp
const auto duplicateID =
    scene.RequestRuntimeActorDuplicate(sourceID);

if (duplicateID.IsValid())
{
    scene.RequestRuntimeActorDestroy(sourceID);
}
```

The remaining update proceeds as follows:

| Phase | Result |
| --- | --- |
| Creation flush | The duplicate copies the still-existing source's current component values |
| Destruction flush | The source's eligible OnDestroy callback runs before the source is removed |
| Physics synchronization | Body bindings are reconciled with the resulting registry |
| Audio update | Source bindings are reconciled with the resulting registry |
| Subsequent script update | The duplicate's script can load and begin if eligible |

The duplicate has a new UUID. Script Actor-reference properties retain their
copied UUID values; they are not automatically redirected to the duplicate.

### Loading a Scene with an unresolved mesh

This illustrative file contains one Actor and no optional scene settings:

```json
{
    "actors": [
        {
            "id": "0000000000000001-0000000000000001",
            "name": "Example",
            "transform": {
                "position": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            },
            "static_mesh": {
                "resource_id": "Cube",
                "visible": true
            }
        }
    ]
}
```

Assume the supplied MeshLibrary has no `Cube` resource.

Loading clears the destination, creates Example with its UUID and Transform,
warns about the missing mesh, and omits StaticMeshComponent. With the remaining
data valid, it returns true and reconstructs the hierarchy from the Actor.

Saving that reconstructed Scene does not preserve the omitted `static_mesh`
section: the component is no longer present.

With Cube loaded before reconstruction, the loader instead adds the mesh
component and connects it to the library's shared resource.

## Current limitations and extension points

Actor lookup currently searches the handle vector linearly.

Explicit-ID creation relies on the caller to supply a valid, unused UUID.
Validation would belong in `Scene::CreateActorWithID()` if this policy changes.

Direct component access modifies registry data. These templates do not
themselves record editor history or explicitly synchronize runtime worlds.
Those integrations require separate documentation.

Persistence currently has no explicit schema version, transactional loading,
or atomic save replacement. Its boolean results do not summarize all skipped
entries or final write failures.

Future persistence changes should review identity validation, resource-loss
diagnostics, exception handling, and compatibility alongside the exact format
reference.