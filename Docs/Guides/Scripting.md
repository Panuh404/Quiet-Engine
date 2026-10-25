# Scripting Guide <!--! {#quiet_guide_scripting} -->

This guide covers assigning Lua scripts, exposing Inspector properties,
using Quiet's bindings, testing callbacks, and diagnosing failures.

## Prerequisites

Open an editable project and import a .lua file through
[Assets](Assets.md).

Add a Script component to an Actor using Inspector's + Add Component,
then choose the imported asset in the Script field.

## First script

Save this content in a Lua source file before importing it:

```lua
local speed = Expose.Float("RotationSpeed", 45.0)

function OnCreate()
    print("Started: " .. Actor.GetName())
end

function OnUpdate(deltaTime)
    local x, y, z = Actor.Transform.GetRotation()
    Actor.Transform.SetRotation(x, y + speed * deltaTime, z)
end

function OnDestroy()
    print("Stopped: " .. Actor.GetName())
end
```

Assign it to a visible Actor with TransformComponent and no rigid body
for this exercise.

RotationSpeed is degrees per second. Set its value under Exposed
Properties, save the Scene, and enter Simulate with F6.

Expected result: the runtime Actor rotates. Lua print() messages go to
standard output, visible in the terminal or IDE output receiving it.
Stop Simulate to return to the unchanged editable Scene.

## Callback names

Define callbacks as environment-level functions rather than local
functions.

| Callback | Arguments | Trigger |
| --- | --- | --- |
| OnCreate | None | Instance startup |
| OnUpdate | deltaTime | Runtime update |
| OnDestroy | None | Destruction of a started instance |
| OnCollisionEnter | Other Actor | Started physics contact |
| OnCollisionExit | Other Actor | Ended physics contact |

Callbacks are optional. There is no required returned script table,
self parameter, or inheritance class.

Use dot calls for bindings, such as Actor.Transform.GetPosition().
Colon syntax would pass an extra argument.

## Exposed properties

Declare properties at file scope using Expose:

| Declaration | Returned value |
| --- | --- |
| Expose.Float("Speed", 1.0) | Number |
| Expose.Int("Count", 3) | Integer |
| Expose.Bool("Enabled", true) | Boolean |
| Expose.String("Label", "Example") | String |
| Expose.Vector2("Offset", 0, 0) | Table with x, y |
| Expose.Vector3("Direction", 0, 1, 0) | Table with x, y, z |
| Expose.Vector4("Value", 0, 0, 0, 1) | Table with x, y, z, w |
| Expose.Color3("Tint", 1, 1, 1) | Table with r, g, b |
| Expose.Color4("TintAlpha", 1, 1, 1, 1) | Table with r, g, b, a |
| Expose.Actor("Target") | Actor reference or nil |
| Expose.Mesh("Mesh") | Asset ID or nil |
| Expose.Texture("Texture") | Asset ID or nil |
| Expose.Audio("Sound") | Asset ID or nil |
| Expose.Script("Behavior") | Asset ID or nil |

Names must be unique and nonempty. Float, vector, and color declaration
defaults must be finite.

After editing declarations, use Refresh Exposed Properties in the Script
component's menu. Review the displayed properties and any source errors,
then save the Scene.

### Discovery and runtime execution

Property discovery executes the file in a temporary Lua state.
Actor, Scene, and Input runtime bindings are absent there.

Keep runtime-dependent work inside callbacks rather than at file scope.
Actor and asset declarations return nil during discovery, so do not
dereference them at file scope.

Runtime Expose functions use a snapshot of the Actor's stored property
values, matching name and type. Editing Lua tables does not update
Inspector values.

Missing value properties use declaration defaults. Missing or unresolved
reference properties return nil.

## Actor and transform API

| Operation | Result |
| --- | --- |
| Actor.IsValid() | Whether the UUID resolves to an existing Actor |
| Actor.GetID() | UUID text |
| Actor.GetName() | Current Actor name |
| Actor.Destroy() | Whether the destruction request was accepted |
| Actor.Duplicate() | Pending duplicate reference or nil |
| Actor.Transform.GetPosition() | Three numbers |
| Actor.Transform.SetPosition(x, y, z) | Success flag |
| Actor.Transform.GetRotation() | Three Euler angles in degrees |
| Actor.Transform.SetRotation(x, y, z) | Success flag |
| Actor.Transform.GetScale() | Three numbers |
| Actor.Transform.SetScale(x, y, z) | Success flag |

Transform setters reject nonfinite values. Scale is clamped per axis
to 0.01–100.

Actor-dependent operations require a valid Actor and the required
components. Check IsValid() before accessing a retained reference.

Transform edits do not teleport physics bodies or rebuild colliders.
Dynamic physics can overwrite Scene position and rotation.

## Text API

Actor.Text controls an existing Text component on the referenced Actor.
Use dot calls inside runtime callbacks:

| Operation | Result |
| --- | --- |
| Actor.Text.HasComponent() | Whether the valid Actor has Text |
| Actor.Text.GetContent() | Copy of the current content; errors if Text is missing |
| Actor.Text.SetContent(content) | Success flag; false if Text is missing |
| Actor.Text.SetVisible(visible) | Success flag; false if Text is missing |
| Actor.Text.SetPixelHeight(height) | Success flag; finite height clamped to [1, 256] |
| Actor.Text.SetColor(r, g, b, a) | Success flag; finite linear RGBA clamped per channel to [0, 1] |

Operations that resolve the Actor raise an error if its UUID no longer
resolves. Check Actor.IsValid() before using a retained Actor reference.
HasComponent() checks component presence, not Actor validity. These
functions do not add a missing Text component.

The size and color setters return false without mutation for nonfinite
input, before Actor lookup. For finite input, they return false when
Text is missing and true after assignment. Height changes scale the
existing bitmap atlas; they do not rerasterize it. Color is straight-alpha
linear RGBA. With natural rectangle dimensions, height changes also
affect the dimensions used for pivot placement.

SetContent copies the supplied string without validating UTF-8. The
renderer skips text whose layout rejects malformed UTF-8. Valid Unicode
characters outside the current atlas use its fallback glyph.

### Runtime timer example

Add Text and Script to the same Actor. Optionally add UI Rectangle to
control placement, as described in the [Editor guide](Editor.md).
Import and assign a Lua file containing:

```lua
local elapsed = 0.0
local previousSecond = -1

function OnCreate()
    if Actor.Text.HasComponent() then
        Actor.Text.SetVisible(true)
        Actor.Text.SetContent("Elapsed: 0 s")
    end
end

function OnUpdate(deltaTime)
    if not Actor.Text.HasComponent() then
        return
    end

    elapsed = elapsed + deltaTime
    local second = math.floor(elapsed)

    if second ~= previousSecond then
        previousSecond = second
        Actor.Text.SetContent("Elapsed: " .. second .. " s")
    end
end
```

Enter Play or Simulate. Expected result: the displayed elapsed time
updates in whole seconds.

The bindings edit the runtime Scene directly without Editor history
entries. Stop runtime to return to the original authored content.
Keep the calls inside callbacks so property discovery can execute the
file without runtime bindings.

## UI Rectangle API

Actor.UIRect controls placement data on the referenced Actor:

| Operation | Result |
| --- | --- |
| Actor.UIRect.HasComponent() | Whether the valid Actor has UI Rectangle |
| Actor.UIRect.GetOffset() | Two pixel coordinates; errors if the component is missing |
| Actor.UIRect.SetOffset(x, y) | Success flag; false for nonfinite input or a missing component |

Offset is unclamped, with positive X rightward and positive Y downward.
SetOffset rejects nonfinite input before Actor lookup. Otherwise the
functions resolve the captured Actor UUID and raise an error if it no
longer exists. They do not create a component. Size, Anchor, and Pivot
currently have Inspector controls but no Lua bindings.

### Runtime movement example

Place Text, UI Rectangle, and Script on the same Actor, and assign a Lua
file containing the following example. Use visible text color and leave
UI Rectangle Size at (0, 0) for natural text dimensions.

```lua
local elapsed = 0.0
local baseX = 0.0
local baseY = 0.0

function OnCreate()
    if not Actor.Text.HasComponent() then
        error("Movement example requires Text on the same Actor.")
    end

    if not Actor.UIRect.HasComponent() then
        error("Movement example requires UI Rectangle on the same Actor.")
    end

    baseX, baseY = Actor.UIRect.GetOffset()
    Actor.Text.SetContent("Moving runtime text")
    Actor.Text.SetVisible(true)
end

function OnUpdate(deltaTime)
    elapsed = elapsed + deltaTime
    Actor.UIRect.SetOffset(
        baseX + math.sin(elapsed * 2.0) * 40.0,
        baseY
    )
end
```

Enter Play or Simulate. Expected result: text moves 40 pixels left and
right of its authored position. Stop runtime to restore authored content
and placement. The script does not move the renderer's fixed panel.

Actor refers to the script's owning Actor, not the selected Actor or a
child. Putting Text on one Actor and UI Rectangle/Script on another does
not connect them. To operate on a different Actor, obtain its reference
through the Scene API and call that reference's Text/UIRect functions.

## Scene API and deferred creation

| Operation | Result |
| --- | --- |
| Scene.FindActorByID(uuidText) | Existing Actor reference or nil |
| Scene.FindFirstActorByName(name) | First matching Actor or nil |
| Scene.CreateActor(name) | Pending Actor reference or nil |

Names are not unique identifiers.

Creation and duplication are deferred. A returned reference can provide
its ID before IsValid() becomes true.

This example waits for materialization before accessing the transform:

```lua
local spawned = nil

function OnCreate()
    spawned = Scene.CreateActor("SpawnedActor")
end

function OnUpdate(deltaTime)
    if spawned and spawned.IsValid() then
        spawned.Transform.SetPosition(0, 2, 0)
        spawned = nil
    end
end
```

An accepted request does not guarantee eventual creation.
Destroying a pending creation reference can cancel that request.

Existing Actor destruction is also deferred. Repeated requests do not
immediately invalidate the Actor reference.

## Input API

| Operation | Result |
| --- | --- |
| Input.Keyboard.IsDown(Key.W) | Held key state |
| Input.Keyboard.IsPressed(Key.Space) | Pressed key state |
| Input.Mouse.IsDown(MouseButton.Left) | Held button state |
| Input.Mouse.IsPressed(MouseButton.Right) | Pressed button state |
| Input.Mouse.GetDelta() | Two movement values |

Key and MouseButton supply named codes. Unsupported numeric codes
return false.

Use Play when testing gameplay input. Editor shortcuts and viewport
navigation can also respond to input, so account for their context.

### Runtime UI and mouse ownership

Ready Editor and Sandbox frames route retained UI pointer input before
runtime Scene input and OnUpdate. A left-button gesture starting on a
participating UI widget is consumed: subsequent
Input.Mouse.IsDown(MouseButton.Left) and
Input.Mouse.IsPressed(MouseButton.Left) return false for that frame.
Ownership persists while dragging outside and through release. Starting
outside and moving into a widget does not transfer that gesture to the UI.

Consumption preserves physical input history, so ending UI ownership does
not synthesize another pressed edge. Cancelling UI interaction keeps the
owned gesture hidden until release is observed. Keyboard queries, other
mouse buttons, pointer movement delta, and earlier input reads are unchanged.

This policy does not retroactively filter OnCreate, OnBegin, or other code
that ran before current-frame UI routing. Use OnUpdate when verifying normal
gameplay input isolation. Retained Button callbacks are currently C++ behavior;
there is no Lua widget lookup or activation-binding API.

See [Application](../Systems/Application.md) for routing and consumption,
and [Renderer](../Systems/Renderer.md) for Button activation contracts.

## Physics API

Actor.RigidBody provides:

- ApplyImpulse(x, y, z).
- SetLinearVelocity(x, y, z).

Both return success flags and require a bound Dynamic body and finite
input. Assign a Dynamic RigidBody and exactly one supported collider
before testing.

Collision callbacks can use the other Actor reference:

```lua
function OnCollisionEnter(otherActor)
    if otherActor.IsValid() then
        print("Contact with " .. otherActor.GetName())
    end
end
```

There is no collision Stay callback, and these callbacks do not receive
contact positions, normals, or impulses.

## Audio API

Actor.Audio controls an existing runtime AudioSource:

| Operation | Behavior |
| --- | --- |
| Play() | Restart from the beginning |
| Stop() | Stop and rewind |
| Pause() | Pause a currently playing source |
| Resume() | Resume a source marked paused |
| Seek(seconds) | Seek to finite, nonnegative seconds |
| GetState() | State string |

Commands return success flags. GetState returns "Unavailable",
"Stopped", "Playing", or "Paused".

These functions do not create missing sources or control Editor previews.

## Errors and hot reload

Quiet logs these errors through its logging system. Lua print() is not
redirected into the Editor Console.

A failed OnCreate leaves an unstarted, faulted instance.
Failed OnUpdate or collision callbacks fault a started instance and
stop its update/contact delivery. Such an instance can still receive
OnDestroy.

Runtime hot reload detects source modification timestamps.

A failed replacement load retains the old instance. Saving again with
a different timestamp triggers another attempt.

Successful replacement creates a fresh environment without migrating
arbitrary Lua state. The old OnDestroy runs before the new OnCreate.

If replacement OnCreate fails, the old instance is not restored.

Initial load failures are different: saving the source alone does not
retry an instance that never loaded. Stop runtime, fix the source, and
restart to attempt initial loading again.

## Verification

- Assign the first script and confirm Exposed Properties discovery.
- Save and reload the Scene to confirm the assignment and values.
- Test OnCreate, OnUpdate, and OnDestroy in Simulate.
- Test input in Play.
- Check returned flags when testing physics or audio.
- Verify reload and error messages using a disposable script.

See [Scripting internals](../Systems/Scripting.md) for lifecycle and
reload details, [Physics](../Systems/Physics.md) for body contracts,
and [Audio](../Systems/Audio.md) for playback behavior.