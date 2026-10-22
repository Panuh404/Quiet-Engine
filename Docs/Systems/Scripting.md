# Scripting <!--! {#quiet_system_scripting} -->

Quiet integrates Lua through a Scene-owned ScriptWorld.

Each ScriptWorld owns a Lua state, runtime script instances, and
records of failed instance creation. Actor instances use individual
environments within that shared state.

This chapter covers ownership, initialization, environments, callback
lifecycle, synchronization, hot reload, property discovery, runtime
bindings, deferred Actor requests, and shutdown.

See [Scene and Actors](Scene.md) for runtime lifecycle,
[Assets](Assets.md) for script asset identity, and
[Physics](Physics.md) for contact-event production.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Runtime world declaration | `Engine/src/Scripting/ScriptWorld.h` |
| Lua state, instances, callbacks, synchronization, and reload | `Engine/src/Scripting/ScriptWorld.cpp` |
| Source resolution and property discovery | `Engine/src/Scripting/ScriptMetadata.h` and `.cpp` |
| Binding declarations | `Engine/src/Scripting/Bindings/Bindings.h` |
| Actor references and Actor API | `Engine/src/Scripting/Bindings/ActorBindings.cpp` |
| Scene API | `Engine/src/Scripting/Bindings/SceneBindings.cpp` |
| Input API | `Engine/src/Scripting/Bindings/InputBindings.cpp` |
| Exposed property values | `Engine/src/Scripting/Bindings/PropertyBindings.cpp` |
| Physics commands | `Engine/src/Scripting/Bindings/PhysicsBindings.cpp` |
| Audio commands | `Engine/src/Scripting/Bindings/AudioBindings.cpp` |
| Authored script and property data | `Engine/src/Scene/Components/ScriptComponent.h` |
| Runtime orchestration and deferred Actor requests | `Engine/src/Scene/Scene.cpp` |

## Ownership

Scene creates ScriptWorld during Play or Simulate runtime startup.

ScriptWorld borrows:

- Its owning Scene.
- ProjectContext.
- Input.

These objects must remain valid while the world uses them.

ScriptWorld owns RuntimeState through a unique pointer.
RuntimeState contains:

- One sol::state.
- Runtime script instances.
- Failed-instance records indexed by Actor UUID.

Instances retain their Actor UUID, script asset ID, source path,
source modification timestamp, environment, optional callbacks,
and lifecycle flags.

The Lua state is shared by instances in that world. Separate instance
environments are not separate Lua virtual machines.

ScriptWorld disables copying and moving.

## Lua initialization

ScriptWorld construction calls Initialize().

Initialization allocates RuntimeState and opens these Lua libraries:

| Library | Purpose |
| --- | --- |
| base | Core Lua operations |
| coroutine | Coroutine operations |
| string | String operations |
| math | Mathematical operations |
| table | Table operations |
| utf8 | UTF-8 operations |

This initialization does not request the io, os, package, or debug
libraries.

The implementation then executes `return _VERSION` through a protected
script call. A failed validation throws std::runtime_error.

After successful validation, it marks the world initialized and logs
the reported Lua version.

An exception inside the initialization try block releases RuntimeState
and propagates to the caller.

IsInitialized() reports world initialization. It does not establish
that every authored script loaded or that runtime callbacks have begun.

## Scene startup ordering

During Scene::BeginRuntime(), the Scene:

1. Creates and builds the physics world.
2. Creates the audio world, applies mixing, and builds sources.
3. Constructs ScriptWorld, initializing its Lua state.
4. Clears pending runtime Actor creation and destruction requests.
5. Sets the runtime mode and invokes the Scene runtime-begin hook.
6. Calls ScriptWorld::BuildInstances().
7. Calls ScriptWorld::Begin().

Script instance construction therefore occurs after physics bodies
and audio sources have been built.

ScriptWorld is created for both Play and Simulate in this implementation.

## Initial instance building

BuildInstances() requires an initialized world with RuntimeState.

It refuses to rebuild after scripting has begun and logs a warning.

Before building, it clears existing instances and failed-instance
records. It then visits the Scene's Actor container and attempts
creation for Actors with a ScriptComponent and nonempty ScriptAssetID.

The method logs loaded and skipped counts and returns void.
Completion does not guarantee that every authored script produced
a runtime instance.

Instance construction and Begin() are separate operations.
Begin() marks the world begun before starting its existing instances.

## Runtime source resolution

ScriptWorld resolves ScriptAssetID through ProjectContext's asset registry.

Resolution requires:

- A nonempty asset ID.
- A registered asset.
- AssetType::Script.
- A nonempty recorded path.
- An available regular file.

Relative paths are joined to the project root. Absolute paths are
retained. The resulting path is lexically normalized.

This resolver does not enforce containment within the project root.

Instance construction also reads the file's last-write timestamp.
If timestamp inspection fails, it records file_time_type::min() and
continues attempting script loading.

## Per-Actor environments

Each instance receives a new Lua environment with fallback to the
world's global table.

Before executing the source file, Quiet installs:

1. Exposed-property bindings.
2. Actor bindings.
3. Scene bindings.
4. Input bindings.

Physics and audio operations are supplied through Actor bindings.

The file is executed using safe_script_file() with text loading mode
and protected error reporting.

Top-level code therefore executes during instance construction,
before OnCreate. Loading is not merely callback discovery.

A reported load error prevents the instance from being stored.
Normal creation attempts record the failed Actor UUID and script
asset ID; successful creation clears that Actor's failure record.

Separate environments do not imply complete isolation: they share
one Lua state and fall back to its global table.

## Callback discovery

After successful file execution, Quiet captures these optional functions:

| Callback | Arguments supplied by Quiet |
| --- | --- |
| OnCreate | None |
| OnUpdate | deltaTime in seconds |
| OnDestroy | None |
| OnCollisionEnter | Other Actor reference |
| OnCollisionExit | Other Actor reference |

Missing or nil callbacks are accepted.

A callback name containing a non-function value produces a warning
and is left uncaptured. It does not reject the whole instance.

Callbacks are captured during construction. Quiet does not look them
up again on every invocation.

## Callback lifecycle

### Starting an instance

StartInstance() skips instances already started or faulted.

If OnCreate exists, it is invoked through a protected call.
Success, or absence of OnCreate, marks the instance started.

An OnCreate failure is logged and marks the instance faulted without
marking it started.

### Runtime updates

ScriptWorld::Update() first synchronizes instance membership and checks
for changed script sources.

Only afterward does it reject nonfinite or negative deltaTime.
Zero is accepted and can be passed to OnUpdate.

OnUpdate runs only for started, non-faulted instances with that callback.

A protected OnUpdate failure is logged and faults the instance.
Later update and collision callbacks skip it.

### Physics contacts

DispatchPhysicsContactEvents() requires an initialized, begun world.

It reads the Scene's buffered events and selects OnCollisionEnter for
Started events or OnCollisionExit for Ended events.

Delivery requires a started, non-faulted instance, the matching callback,
and an existing other Actor. Each event is dispatched to Actor A with
Actor B, then to Actor B with Actor A.

A protected collision-callback failure faults the receiving instance.

These callbacks run after the Scene's physics update, rather than
inside Jolt contact callbacks. See [Physics](Physics.md) for event
buffering and Scene delivery order.

### Destroying an instance

DestroyInstance() invokes OnDestroy only when the instance is started
and the callback exists.

It does not exclude started instances merely because they are faulted.
An instance faulted by OnUpdate or a collision callback can therefore
still receive OnDestroy.

An instance whose OnCreate failed remains unstarted and does not
receive OnDestroy through this path.

A protected OnDestroy failure is logged, but the instance is still
removed and its failed-instance record cleared.

NotifyActorWillBeDestroyed() removes the matching instance through this
same destruction path. If no instance exists, it clears the Actor's
failure record.

## Instance synchronization

ScriptWorld::Update() synchronizes instance membership before checking
source changes and before validating deltaTime.

SynchronizeInstances() removes an instance when:

- Its Actor UUID no longer resolves.
- ScriptComponent is missing.
- ScriptAssetID is empty.
- ScriptAssetID differs from the instance's recorded asset ID.

Removal uses DestroyInstance(), including its OnDestroy conditions.

The method then visits Scene Actors and creates instances for assigned
scripts that have neither an existing instance nor a matching
failed-creation record.

Newly created instances are immediately passed to StartInstance().
They can receive OnUpdate later in the same Update() call when the
delta is accepted and startup succeeds.

The logged created count describes constructed instances, not a
guarantee that every OnCreate succeeded.

### Failed initial loads

Failed creation records are keyed by Actor UUID and contain the
attempted script asset ID.

While an Actor retains the same assignment, synchronization suppresses
another creation attempt. Editing or saving that source file alone
does not clear this suppression.

The record is cleared when synchronization observes a missing component,
empty assignment, or different script asset ID. Actor destruction and
world lifecycle cleanup also clear records.

Rebuilding instances before Begin(), or restarting runtime with a new
world, provides another initial creation attempt.

A callback failure differs from a creation failure: it leaves a faulted
instance in the instance list rather than creating a failed-load record.

### Existing-instance edits

Changing ScriptAssetID replaces the instance through removal and
creation. This does not preserve the old Lua environment.

Changing ScriptComponent.Properties alone does not rebuild an existing
instance or reinstall its property bindings through synchronization.

## Hot reload detection

ReloadChangedInstances() checks the last-write timestamp of each
retained instance's recorded source path.

An unchanged timestamp or filesystem inspection error skips the attempt.
Quiet does not compare file contents or hashes.

A registry path change alone does not redirect this timestamp check:
it continues watching the recorded source path. Replacement construction,
when triggered, resolves the current asset record again.

The new timestamp is recorded before attempting replacement. A failed
attempt is therefore not repeated every frame for that same timestamp.

Saving again can trigger another attempt when the filesystem reports
a different timestamp.

Instances that failed initial creation are absent from this list and
are not recovered by this reload loop.

## Hot reload replacement

When a source timestamp changes, Quiet:

1. Records the timestamp being attempted.
2. Resolves the instance's Actor.
3. Constructs a replacement environment and executes its source.
4. Keeps the old instance if replacement construction fails.
5. On successful construction, destroys the old instance.
6. Inserts the replacement at the old instance's position.
7. Starts the replacement through StartInstance().

The replacement source executes at file scope before the old
instance's OnDestroy callback.

### State and failure boundaries

Successful replacement creates a fresh environment and captures new
callbacks. Quiet does not migrate arbitrary Lua instance state from
the old environment.

A failed replacement load retains the previous instance and environment.
It does not undo side effects already performed by the attempted
replacement's top-level code.

Once replacement construction succeeds, the old instance is destroyed
before the new OnCreate runs. A failing replacement OnCreate faults
the new instance; the old instance is not restored.

The reload success log indicates that replacement construction and
installation completed. It does not guarantee that OnCreate succeeded.

The reload loop does not exclude faulted instances. A subsequent source
change can therefore replace an instance faulted by an earlier callback.

## Binding structure

Runtime environments receive these tables:

| Table | Responsibility |
| --- | --- |
| Actor | Reference to the instance's owning Actor |
| Actor.Transform | Scene transform reads and writes |
| Actor.Text | Existing Text component content, visibility, size, and color |
| Actor.UIRect | Existing UI Rectangle offset reads and writes |
| Actor.RigidBody | Physics impulse and velocity commands |
| Actor.Audio | Playback commands and state queries |
| Scene | Actor lookup and runtime creation requests |
| Input.Keyboard | Keyboard state queries |
| Input.Mouse | Mouse state queries and movement delta |
| Key | Named keyboard codes |
| MouseButton | Named mouse-button codes |
| Expose | Authored property values and declaration defaults |

Bindings capture Scene references and Actor UUIDs rather than retaining
direct pointers to Actors.

Actor.IsValid() resolves the UUID against the current Scene.
Actor.GetID() returns the captured UUID as text.

Operations such as Actor.GetName() and transform access require the Actor
to remain available and raise an error when it no longer resolves.

Physics and audio bindings forward commands through Scene and inherit
the runtime contracts documented in their respective chapters.

Binding functions use dot-call syntax, such as
Actor.Transform.GetPosition().

### Text component bindings

CreateActorReference() in Engine/src/Scripting/Bindings/ActorBindings.cpp
installs Actor.Text on Actor references, including references returned by
Scene lookups. Its functions capture the existing requireActor closure,
which borrows the Scene and resolves the captured UUID on every call.
They retain no Actor or component pointers between calls and do not keep
the Actor alive.

HasComponent() checks for TextComponent after resolving the Actor.
GetContent() returns a string copy and raises an error if Text is absent.
SetContent() copies into TextComponent::Content, and SetVisible() assigns
TextComponent::Visible. Both setters return false if Text is absent and
true after assignment. These four functions raise the existing unavailable
Actor error if UUID lookup fails; HasComponent() is not a validity query.

SetPixelHeight() and SetColor() reject nonfinite inputs before resolving
the Actor. For finite inputs they resolve the Actor, return false for
missing Text, and otherwise assign height clamped to [1, 256] or
straight-alpha linear RGBA clamped per channel to [0, 1]. They return
true after assignment, including assignments equal to existing values.
An unresolved Actor raises the same error when lookup is reached.

The Scene owns the component and its content. These bindings directly
mutate Scene data without Editor commands or history. Editor runtime
uses a cloned Scene, so stopping runtime discards these changes and
preserves the editable Scene.

The renderer reads visibility and content and builds text layout each
frame. The bindings create no font resources or layout caches.
SetContent() accepts string bytes without UTF-8 validation; malformed
UTF-8 causes the renderer to skip that text when layout fails. Valid
unsupported codepoints use the atlas fallback glyph.

### UI rectangle bindings

The same CreateActorReference() function installs Actor.UIRect.
HasComponent() checks UIRectComponent after UUID resolution. GetOffset()
returns a tuple of copied X/Y values and raises an error if the component
is missing. SetOffset() rejects nonfinite inputs before Actor lookup;
otherwise it returns false for a missing component and assigns finite,
unclamped pixel coordinates on success. An unresolved Actor raises an
error whenever UUID lookup is reached.

These closures retain no component references and create no missing
components. They edit runtime Scene data directly, without Editor history
or 3D transform changes. The renderer resolves placement each frame from
Anchor, Offset, Pivot, and authored/natural dimensions. There are no Lua
bindings for Size, Anchor, or Pivot yet.

The owning Actor reference does not search other Actors for components.
A script using Actor.Text and Actor.UIRect needs those components on its
own Actor. Other Actor references obtained through Scene lookup expose
the same tables and resolve their own captured UUIDs.

The [Scripting guide](../Guides/Scripting.md) documents signatures,
validation results, and timer/movement examples.

## Exposed property types

ScriptComponent stores the assigned script asset ID and a vector of
typed ScriptProperty values.

The Expose table supports:

| Function | Runtime result |
| --- | --- |
| Float | Floating-point value |
| Int | Integer value |
| Bool | Boolean value |
| String | String value |
| Vector2 | Table with x and y |
| Vector3 | Table with x, y, and z |
| Vector4 | Table with x, y, z, and w |
| Color3 | Table with r, g, and b |
| Color4 | Table with r, g, b, and a |
| Actor | Actor reference or nil |
| Mesh, Texture, Audio, Script | Asset ID string or nil |

Value declarations take a name and default value or components.
Reference declarations take a name without a default reference.

## Property discovery

DiscoverScriptProperties() resolves the script source and creates a
temporary Lua state with the same six standard libraries used by
ScriptWorld initialization.

It installs a discovery Expose table and replaces print with a no-op
to avoid file-scope output reaching the Editor console.

Discovery executes the source as text. It does not invoke the runtime
lifecycle callbacks.

The discovery environment does not install Actor, Scene, or Input
runtime bindings. Top-level code that requires those bindings can
therefore fail during discovery.

### Declaration validation

Property names must be nonempty and unique across declarations,
including declarations of different types.

Float, vector, and color defaults must be finite. Discovery does not
clamp color components to the range 0–1.

Scalar declarations return their defaults. Vector and color declarations
return tables containing their default components.

Actor and asset declarations record the property type and return nil.

A protected script-execution failure or a recorded declaration error
clears the discovered property list and returns false with an error.

Discovery is execution-based rather than static source parsing.
Only declarations reached by that execution are collected.

## Runtime property values

InstallPropertyBindings() copies ScriptComponent.Properties into a
shared immutable snapshot retained by the installed functions.

Lookup matches both property name and type.

Editing the component after installation does not update that snapshot.
A newly constructed or reloaded instance installs a new snapshot.

Scalar, vector, and color declarations use the matching authored value
when present, otherwise the supplied declaration default.

Float additionally rejects a nonfinite authored value. It uses a finite
default, or zero if that default is also nonfinite.

Runtime vector and color bindings do not repeat discovery's finite-value
validation.

Vector and color results are newly created Lua tables. Editing those
tables does not write changes back to ScriptComponent.

### Actor references

Expose.Actor() returns nil when its property is missing, its UUID is
invalid, or the referenced Actor does not currently exist.

Otherwise, it creates an Actor reference using that UUID.
The reference does not keep the Actor alive.

### Asset references

Asset declarations require a matching typed property, a nonempty asset
ID, and a registry record of the expected asset type.

Success returns the asset ID string.

This check establishes registry identity and type. It does not establish
that the asset's source file or runtime resource is available.

## Scene lookup and deferred Actor requests

Scene.FindActorByID() accepts UUID text and returns nil when parsing
fails or the Actor does not exist.

Scene.FindFirstActorByName() returns the first matching Actor in
container order, or nil. Names are not unique identifiers.

### Creating and duplicating Actors

Scene.CreateActor(name) requests runtime creation and returns an Actor
reference for the reserved UUID, or nil when the request is rejected.
An empty name becomes "Actor".

Actor.Duplicate() requests duplication of an existing Actor and returns
a reference to the reserved duplicate UUID, or nil.

These references can exist before their Actors are materialized.
IsValid() remains false until the Scene creates the Actor.
GetID() can still return the reserved UUID.

Duplication copies the source during the creation flush rather than
capturing a snapshot when the request is submitted. If the source is
unavailable then, creation can be skipped.

An accepted request therefore does not guarantee successful
materialization.

### Destruction requests

Actor.Destroy() forwards a runtime destruction request.

For an existing Actor, destruction is queued. Repeating an already
queued request returns true without adding another entry.

For a reference to a pending creation, destruction cancels that
creation request.

A true result indicates request acceptance or cancellation, rather
than immediate destruction of an existing Actor.

### Flush ordering

Scene::UpdateRuntime() flushes Actor creations and then destructions
after script updates, physics stepping, and contact dispatch.

Creation processing takes a snapshot of its current request queue.

Destruction processing can include additional destruction requests
submitted by OnDestroy callbacks during that same phase.

Before removing an Actor, Scene notifies ScriptWorld so OnDestroy can
run while the Actor and native runtime worlds still exist.

Newly created scripted Actors are discovered by ScriptWorld on the
next synchronization pass. Physics membership is synchronized again
after the flushes; audio synchronization follows in the runtime update.

See [Scene and Actors](Scene.md) for the complete mutation lifecycle.

## Transform bindings

Actor.Transform exposes position, rotation, and scale through
GetPosition/SetPosition, GetRotation/SetRotation, and GetScale/SetScale.

Getters return three Lua values rather than a vector table.
Rotation values are Euler angles in degrees.

Setters reject nonfinite components. Position and rotation are assigned
directly; scale components are clamped to 0.01–100.

These operations require an available Actor and TransformComponent.

They edit WorldTransform rather than issuing a physics teleport or
collider rebuild. Dynamic-body synchronization can overwrite authored
position and rotation, and existing collider dimensions can remain
unchanged. See [Physics](Physics.md) for transform authority.

## Input bindings

Input.Keyboard.IsDown() and IsPressed() forward to Quiet's keyboard
state queries.

Input.Mouse.IsDown() and IsPressed() forward to mouse-button queries.

Unsupported numeric key or button codes return false.
Key and MouseButton provide the named codes installed by the bindings.

Input.Mouse.GetDelta() returns two Lua values for mouse movement.

Bindings borrow the application's Input object. They do not maintain
independent per-script input history.

## Physics and audio forwarding

Actor.RigidBody.ApplyImpulse(x, y, z) and
Actor.RigidBody.SetLinearVelocity(x, y, z) forward through Scene.

Their actual-body requirements and return behavior are documented in
[Physics](Physics.md).

Actor.Audio exposes Play, Stop, Pause, Resume, Seek, and GetState.
Playback commands return the Scene operation's success flag.
Seek rejects negative or nonfinite seconds.

GetState returns one of these strings:

- "Unavailable"
- "Stopped"
- "Playing"
- "Paused"

See [Audio](Audio.md) for restart, rewind, pause, seek, and failure
semantics. These bindings control the Actor's runtime source rather
than subsystem one-shots or editor previews.

## Shutdown

Scene::EndRuntime() calls ScriptWorld::End() before releasing the
script world, audio world, and physics world.

End() destroys retained instances in reverse instance order, clears
failed-instance records, and resets the begun flag.

ScriptWorld destruction calls Shutdown(), which calls End() and
releases instance-held Lua references before releasing RuntimeState.

Shutdown resets both initialized and begun flags.

The detailed conditions for invoking OnDestroy are documented with
the callback lifecycle rather than implied by world destruction alone.