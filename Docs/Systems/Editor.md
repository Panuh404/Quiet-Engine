# Editor <!--! {#quiet_system_editor} -->

Quiet's Editor system coordinates authoring state, Scene mutations,
history, Scene file operations, and runtime transitions.

Application owns the main Editor controllers and workflows. Renderer UI
code presents the controls, while Editor objects provide state and behavior.

This chapter covers responsibilities, ownership, requests versus commands,
mutation boundaries, history, dirty-state tracking, Scene persistence,
session resets, and runtime transitions.

See [Application](Application.md) for startup and frame orchestration,
and [Scene and Actors](Scene.md) for the underlying Scene contracts.

## Implementation map

| Responsibility | Declaration | Implementation |
| --- | --- | --- |
| Editor state and request queue | `Engine/include/Quiet/Editor/EditorState.h` | Inline in the declaration |
| Controller and viewport interaction | `Engine/include/Quiet/Editor/EditorController.h` | `Engine/src/Editor/EditorController.cpp` |
| Scene mutation commands | `Engine/include/Quiet/Editor/EditorCommands.h` | `Engine/src/Editor/EditorCommands.cpp` |
| Scene history | `Engine/include/Quiet/Editor/EditorSceneHistory.h` | `Engine/src/Editor/EditorSceneHistory.cpp` |
| Scene file workflow | `Engine/include/Quiet/Editor/EditorSceneWorkflow.h` | `Engine/src/Editor/EditorSceneWorkflow.cpp` |
| Runtime transitions | `Engine/include/Quiet/Editor/EditorRuntimeController.h` | `Engine/src/Editor/EditorRuntimeController.cpp` |
| Request dispatch and frame integration | `Engine/include/Quiet/Core/Application.h` | `Engine/src/Core/Application.cpp` |

## Responsibilities

### EditorState

Quiet::Editor::EditorState stores the shared authoring state.

Its sections include:

- Selected, hovered, and piloted Actor identities.
- Viewport dimensions, interaction state, and display options.
- Gizmo operation, coordinate mode, and snapping configuration.
- Scene mode, dirty state, and display name.
- Undo and redo availability.
- Script source diagnostics.
- Project-manager input buffers.
- Panel focus and asset-operation feedback.
- Pending Editor requests.

Selection stores optional Actor UUIDs. It does not own Actors or retain
Actor pointers.

State describes the Editor session. Assigning a state field does not
automatically execute the workflow associated with that field.

### EditorController

Quiet::Editor::EditorController owns EditorState and EditorSceneHistory.

It exposes mutable and const access to the state and coordinates selection,
history operations, shortcuts, viewport picking, gizmos, camera focus,
debug icons, and script source synchronization.

It also stores script source modification times and whether a gizmo was
being used.

The controller does not own the Scene. Its operations receive the Scene
and other required systems as arguments.

### EditorCommands

Quiet::Editor::EditorCommands provides operations for authoring Actors,
hierarchy entries, Scene settings, and components.

It stores a reference to EditorController. It receives the target Scene
and any additional resources through operation arguments.

EditorCommands is distinct from the Editor request queue. Its methods
are callable operations; they are not queued request records.

See the Scene mutation boundaries section for interaction grouping and
operation-specific return-value contracts.

### EditorSceneWorkflow

Quiet::Editor::EditorSceneWorkflow coordinates default Scene creation,
saving, loading, opening a project Scene, and resetting the Editor Scene
session.

It borrows references to:

- The editable main Scene.
- ProjectContext.
- The renderer's MeshLibrary.
- EditorController.

Its normal new, save, load, and open operations check that the Editor is
in Edit mode before proceeding.

The workflow delegates Scene data persistence to SceneDocument.
It does not own a separate serializer or Scene.

### EditorRuntimeController

Quiet::Editor::EditorRuntimeController coordinates entering and stopping
Simulate and Play.

It borrows the editable Scene, EditorController, physics subsystem,
audio subsystem, project context, and Input.

It also stores a reference to Application's runtime Scene unique pointer.
This allows the controller to create and release the runtime Scene while
Application retains the owning pointer as part of its state.

Mouse capture is accessed through setter and getter callbacks supplied
by Application.

## Ownership and dependency lifetime

| Object | Owned state | Borrowed dependencies |
| --- | --- | --- |
| Application | Editor controllers, workflows, main Scene, and runtime Scene pointer | Coordinates the engine systems |
| EditorController | EditorState, EditorSceneHistory, and script tracking state | Systems passed to individual operations |
| EditorCommands | No Scene or controller ownership | EditorController; operation arguments |
| EditorSceneWorkflow | No Scene ownership | Main Scene, project context, mesh library, and controller |
| EditorRuntimeController | Stored callbacks and capture state | Controller, Scenes, runtime Scene pointer, subsystems, context, and Input |

Borrowed references require their dependencies to outlive the objects
and operations that use them.

Application's explicit shutdown releases workflows before the Scene and
libraries they reference. Runtime termination occurs before the physics
and audio subsystems are destroyed.

## Requests and commands

EditorState::QueueRequest() appends an EditorRequest to PendingRequests.

Each request contains a type and an optional string argument.
PopNextRequest() removes the oldest request, or returns an empty optional
when the queue is empty.

Queueing means that the request has been stored. It does not establish
that its action has executed or succeeded.

Application::ProcessEditorRequests() removes and dispatches pending
requests during the ready Editor frame path.

EditorCommands serves a different purpose: it provides Scene mutation
operations used by authoring tools and request handlers.

A request can lead to a command or workflow call, but these are separate
mechanisms. Their mode checks, mutation handling, and failure behavior
must be understood at the operation being executed.

## Scene mutation boundaries

EditorCommands supports immediate operations and operations intended
to participate in an enclosing interaction.

These have different history and return-value contracts.

### Immediate operations

The implementation helper ApplyImmediateMutation() performs this sequence:

1. Rejects the operation outside Edit mode.
2. Calls EditorController::BeginSceneMutation().
3. Executes the supplied mutation.
4. Calls EditorController::EndSceneMutation().
5. Returns true only when both the mutation and history commit report true.

CreateEmptyActor(), DuplicateActor(), and DeleteActor() use this helper.

An immediate command's false result is not a general rollback guarantee.
The helper does not restore the previous Scene when the mutation reports
false, and it does not catch exceptions.

### UI rectangle authoring commands

AddUIRect() and RemoveUIRect() use immediate history mutations.
Adding or removing a rectangle does not add or remove TextComponent.

SetUIRectOffset(), SetUIRectAnchor(), and SetUIRectPivot() require an
enclosing interaction boundary. The Inspector begins before applying
widget changes and ends after deactivation.

Commands reject nonfinite coordinates. Offset remains unclamped;
Anchor and Pivot are clamped to [0, 1]. All commands reject authoring
outside Edit mode. Scene-file loading retains its existing unclamped
behavior.

### Text authoring commands

AddText() and RemoveText() use ApplyImmediateMutation() for Edit-mode
authoring with immediate history tracking.

SetTextContent() changes component content but does not open or close
a history boundary. The multiline Text Inspector brackets the edit
through TrackedInspectorInteraction: it begins before applying changed
content and ends after the widget deactivates.

The Inspector uses a local string while drawing the widget, leaving
Scene mutation to EditorCommands. It returns immediately after component
removal so it does not reuse the removed component pointer.

SetTextPixelHeight() and SetTextColor() require an enclosing interaction
boundary. The Inspector begins that boundary before applying widget
changes and ends it after deactivation.

Pixel-height commands reject nonfinite input and clamp accepted values
to [1, 256]. Color commands reject nonfinite channels and clamp accepted
linear RGBA values to [0, 1]. Scene-file loading does not apply these
Editor-command limits.

SetTextVisible() uses ApplyImmediateMutation() to record a discrete
visibility change. All text authoring commands reject changes outside
Edit mode.

### Interaction operations

BeginInteraction() starts a history boundary in Edit mode.
EndInteraction() closes that boundary and returns the history commit result.

Methods such as RenameActor() and SetActorPosition() modify the Scene
without opening and closing their own history boundary.

Their callers must supply an interaction boundary when the edit should
participate in undo/redo and dirty-state refresh.

This allows a continuous UI interaction to produce one history entry
instead of one entry for every intermediate value.

### Return values are operation-specific

| Operation | Meaning of true |
| --- | --- |
| CanEditScene() | Editor state currently selects Edit mode |
| RenameActor() | An existing Actor's name was changed |
| SetActorPosition() | A finite position was assigned to an existing Actor |
| EndInteraction() | A pending snapshot differed from the final snapshot and was added to undo history |
| Immediate mutation helper | Both its mutation and history commit reported true |

SetActorPosition() can return true when assigning the existing position.
It does not compare the old and new values.

The history comparison can still treat that interaction as unchanged.

## History storage

Quiet::Editor::EditorSceneHistory records Scene snapshots rather than
inverse command objects.

It maintains:

- An undo stack.
- A redo stack.
- One pending pre-mutation snapshot.
- One clean snapshot used for dirty-state comparison.

Each undo and redo stack is limited to 128 entries. Pushing beyond that
limit removes the oldest entry from the corresponding stack.

### Snapshot contents

A Scene snapshot contains:

- Actor UUIDs, names, and Transform components.
- Supported optional component values.
- Scene hierarchy.
- Audio, physics, and render settings.
- The selected Actor UUID.

Supported optional components currently include StaticMesh, Material,
DirectionalLight, Rotator, Camera, AudioSource, AudioListener, Script,
RigidBody, BoxCollider, SphereCollider, and CapsuleCollider.

The snapshot does not capture the entire EditorState. Viewport options,
gizmo configuration, hovered Actor, and piloted camera are not restored
by RestoreSnapshot().

Snapshots copy component values. This does not imply deep copying of
resources referenced through shared pointers.

Adding a component to the Scene does not automatically add it to history.
Capture, restoration, and equality comparison must explicitly support it.

## Recording a mutation

BeginMutation() does nothing outside Edit mode.

When no pending snapshot exists, it captures the current Scene and
selected Actor. Further begin calls retain that original snapshot.

There is one pending boundary, not a stack or nesting counter.
The first end call consumes it.

EndMutation() returns false when no pending snapshot exists.

Otherwise, it captures the final state and compares it with the pending
snapshot.

| Comparison | Result |
| --- | --- |
| Equal | Discards the pending snapshot, refreshes dirty state, and returns false |
| Different | Pushes the previous state onto undo, clears redo, refreshes flags and dirty state, and returns true |

BeginMutation() checks Edit mode. EndMutation() itself does not repeat
that check; EditorCommands::EndInteraction() does.

The mutation boundary records changes. It does not provide automatic
exception rollback or transaction isolation.

## Undo and redo

Undo() and Redo() reject execution outside Edit mode or when their
required stack is empty. They refresh history availability flags and
return false in those cases.

A successful undo:

1. Captures the current state onto redo.
2. Removes the newest undo snapshot.
3. Restores that snapshot.
4. Clears the pending mutation.
5. Refreshes history flags and dirty state.

Redo performs the corresponding operation in the opposite direction.

### Restoration and reference lifetime

RestoreSnapshot() clears the destination Scene and reconstructs Actors
using the recorded UUIDs.

It restores supported components, hierarchy, Scene settings, and selection.

Existing Actor pointers, references, and entity handles must not be reused
after restoration. Resolve Actors again by UUID.

Restoration is not an atomic swap. Exceptions during reconstruction can
leave partial Scene state, and the history operation does not provide
rollback around that process.

### Availability flags

History.CanUndo and History.CanRedo indicate whether their stacks contain
entries.

They do not independently express whether undo or redo is permitted in
the current Scene mode. The operation also checks Edit mode.

## Dirty state

The clean snapshot is a comparison baseline.

Reset() clears undo, redo, and the pending mutation, then captures the
current Scene as clean.

MarkSaved() replaces the clean baseline and refreshes dirty state.
It does not clear undo, redo, or the pending mutation, and it does not
write a Scene file itself.

EditorSceneWorkflow calls MarkSceneSaved() after SceneDocument reports
a successful save.

### History equality versus dirty equality

History commit comparison includes the selected Actor UUID.
Dirty-state comparison excludes selection.

Consequently, a selection-only difference inside a mutation boundary can
produce a history entry without marking Scene content dirty.

Changing selection outside a mutation boundary does not automatically
create a history entry.

Dirty comparison includes Actor content and ordering, hierarchy, and
audio, physics, and render settings.

Returning to content equal to the clean baseline can clear Dirty even
when undo or redo entries remain.

### Comparison tolerance

The floating-point comparison helper uses an absolute tolerance of 0.0001.
Supported vector comparisons apply it to their individual components.

A small numeric change can therefore modify the Scene while still being
treated as equal by history and dirty-state comparisons.

StaticMesh comparison checks resource identity, visibility, and material
slots. It does not compare the contents of the referenced mesh resource.

### Refresh timing

Dirty state is refreshed by history operations such as Reset(),
MarkSaved(), EndMutation(), Undo(), and Redo().

Direct Scene mutation does not automatically notify EditorSceneHistory.
An edit performed outside the expected history boundary can leave the
displayed dirty state stale until a later refresh.

## Scene persistence workflows

Quiet::Editor::EditorSceneWorkflow operates on the editable main Scene
and the active Scene path stored in ProjectContext.

NewScene(), SaveScene(), LoadScene(), and OpenSceneInProject() require
Edit mode. EnsureEditMode() logs a warning and rejects other modes.

These workflow methods do not themselves prompt about unsaved changes.

### Creating a new Scene

NewScene() performs these operations:

1. Creates the default Scene through SceneDocument::CreateDefault().
2. Resets the active Scene path to the project's startup Scene path.
3. Resets the Editor Scene session.

It does not save a file or choose a new unique filename.

The session reset establishes the new Scene as the clean history baseline.
Clean therefore does not necessarily mean that the current content has
already been written to disk.

Exceptions propagate. In particular, default Scene construction can fail
after clearing the destination.

### Saving

SaveScene() first requires Edit mode and successful project directory
creation.

It saves to ProjectContext::GetActiveScenePath() through SceneDocument.

When saving reports true, the workflow updates the display name and calls
EditorController::MarkSceneSaved().

This updates the clean history baseline without clearing undo or redo.
Saving is not a history reset.

The workflow inherits the serializer's save-result limitations: true means
the serializer issued its write, not that final stream status was checked.

### Loading

LoadScene() loads the active Scene path through SceneDocument.

A false result returns false without resetting the Editor session.
A true result resets the session and returns true.

The method does not create a default Scene as a fallback.
That fallback belongs to the editable project startup helper.

Loading reconstructs the destination in place. A failure or exception is
not a guarantee that the previous Scene content remains intact.

See [Scene serialization](Scene.md) for partial-state
and exception contracts.

### Opening another project Scene

OpenSceneInProject() saves the previous active path, assigns the requested
path, and calls LoadScene().

| Load outcome | Active path behavior |
| --- | --- |
| True | Keeps the requested path |
| False | Restores the previous path |
| Exception | Restoration code is not reached |

Restoring the path after a false result does not roll back Scene content.

ProjectContext resolves relative paths beneath the project's Scenes
directory and accepts absolute paths directly. The method's parameter
name does not by itself enforce project containment.

## Scene session resets

ResetSceneSession() performs these operations:

1. Updates the display name from the active Scene filename.
2. Clears the piloted camera UUID.
3. Clears selected and hovered Actor UUIDs.
4. Resets Scene history against the current editable Scene.

EditorController::ResetSceneHistory() also clears tracked script source
timestamps and script source diagnostics.

The history reset clears undo, redo, and the pending mutation, then
establishes the current Scene as clean.

ResetSceneSession() is a targeted reset. It does not replace the entire
EditorState or clear all viewport, gizmo, feedback, and request state.

Neither ResetSceneSession() nor UpdateSceneDisplayName() independently
checks Edit mode.

## Play and Simulate transitions

Quiet::Editor::EditorRuntimeController creates a separate Scene copy
for Editor runtime.

| Operation | Required current mode | Result after normal completion |
| --- | --- | --- |
| EnterSimulateMode() | Edit | Simulate |
| StopSimulateMode() | Simulate | Edit |
| EnterPlayMode() | Edit | Play |
| StopPlayMode() | Play | Edit |

Calls from other modes return without performing the transition.

Switching directly from Play to Simulate, or Simulate to Play, requires
stopping the current mode first.

### Creating the runtime Scene

Entering either runtime mode first clears the piloted camera UUID.

CreateRuntimeScene() then:

1. Destroys any existing runtime Scene.
2. Stops audio preview.
3. Copies the editable Scene into the externally owned runtime Scene pointer.
4. Calls BeginRuntime() with the requested runtime mode and dependencies.

The controller sets EditorState's mode after CreateRuntimeScene() returns.

Scene copying preserves Actor identities and copies supported component
data, while referenced resources can remain shared.

See [Scene and Actors](Scene.md) for Scene-copy and runtime contracts.

### Mouse capture

After creating Play runtime, EnterPlayMode() records the current mouse
capture value and requests capture.

StopPlayMode() destroys runtime, selects Edit mode, and restores that
recorded capture value.

Simulate entry and exit do not perform this capture save-and-restore
sequence.

Application's EnterPlay request handler ends Editor viewport navigation
capture before calling EnterPlayMode(). That preparation belongs to the
request handler, not the runtime controller method itself.

### Stopping runtime

DestroyRuntimeScene() ends runtime when active, then releases the runtime
Scene pointer.

Stopping Play or Simulate discards the runtime Scene. The controller does
not merge its changes into the editable Scene or record them as editable
Scene history.

The controller's transition methods do not reset editable Scene history,
clear selection, or restore a previously piloted camera.

Resource sharing still follows the Scene-copy contract. Discarding the
runtime Scene is not a rollback of arbitrary changes to shared resources.

### Active Scene selection

GetActiveScene() returns the runtime Scene when the state is non-Edit
and the runtime pointer exists.

Otherwise, it returns the editable Scene.

This fallback does not repair a missing runtime Scene or begin runtime.

### Shutdown versus stopping Play

Shutdown() destroys the runtime Scene.

It does not independently change EditorState's mode or restore the mouse
capture value saved on Play entry.

Application uses this teardown during shutdown, before destroying runtime
dependencies.

### Exception behavior

Runtime transitions do not catch exceptions or implement rollback.

For example, BeginRuntime() can throw after the runtime copy has been
assigned but before EditorState's mode changes.

A transition therefore does not provide an all-or-nothing guarantee for
Scene allocation, runtime initialization, mode state, or mouse capture.

## Worked example: recording an interaction

The following code assumes an initialized editable Scene, an existing
EditorController, and the UUID of an Actor in that Scene.

It illustrates a history boundary around a setter that does not record
history independently.

```cpp
// Existing context:
// Quiet::Scene::Scene& scene;
// Quiet::Editor::EditorController& controller;
// Quiet::Core::UUID actorID;

Quiet::Editor::EditorCommands commands(controller);

if (commands.CanEditScene())
{
    commands.BeginInteraction(scene);

    const bool assigned =
        commands.SetActorPosition(scene, actorID, glm::vec3(2.0f, 0.0f, 0.0f));

    const bool recorded = commands.EndInteraction(scene);

    // assigned reports whether the setter accepted the assignment.
    // recorded reports whether history detected a differing snapshot.
}
```

EndInteraction() is called even when the setter returns false, so the
pending boundary is consumed.

A true assignment does not guarantee a history entry. Assigning the
existing position, or a change within comparison tolerance, can produce
assigned == true and recorded == false.

After a recorded interaction, controller.Undo(scene) can reconstruct the
previous snapshot when the Editor remains in Edit mode.

Resolve Actors again by UUID after undo or redo. Do not retain an Actor
pointer across snapshot restoration.

This example does not add exception handling. If an operation throws
before EndInteraction(), the pending boundary is not automatically closed.

## Extension guidance

### Adding a component to history

Update all three parts of EditorSceneHistory:

1. Snapshot storage and CaptureSnapshot().
2. RestoreSnapshot().
3. Equality comparison used by history and dirty tracking.

Capturing a component without comparing it can hide changes.
Comparing it without restoring it can make undo incomplete.

Use the component's ownership contract when deciding whether snapshot
copying should share or duplicate referenced resources.

### Adding an authoring command

Choose whether the operation records its own immediate mutation or
participates in a caller-managed interaction.

Document its mode requirement and exact return-value meaning.
Do not assume that every bool-returning command reports a changed value
or a committed history entry.

Use Actor UUIDs for later lookup rather than retaining Actor references
across creation, deletion, or history restoration.

### Adding a queued request

Add its request type and dispatch behavior, including argument handling,
mode restrictions, and failure feedback.

Queue insertion is not execution success. The dispatch path must define
what happens when dependencies are unavailable or an operation fails.

### Changing runtime transitions

Preserve the distinction between the editable Scene and runtime copy,
and define how mouse capture and borrowed dependencies are handled.

A change to transition ordering also changes which partial states can
remain when an operation throws.

### Current scope

This chapter documents the Editor's coordination, mutation, history,
persistence, and runtime contracts.

Panel controls and user-facing workflows are documented in the
[Editor guide](../Guides/Editor.md). Individual component commands, viewport picking algorithms,
gizmo details, and script reconciliation can receive more detailed API
documentation as those systems are expanded.