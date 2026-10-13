# Physics <!--! {#quiet_system_physics} -->

Quiet integrates Jolt through an application-owned PhysicsSubsystem
and Scene-owned PhysicsWorld objects.

The subsystem manages shared Jolt infrastructure. Each runtime world
manages its simulation, body bindings, and contact events.

This chapter covers physics ownership, initialization, fixed-step
timing, body eligibility, collider construction, membership
synchronization, transform authority, runtime commands, collision
filtering, contact delivery, and teardown.

See [Application](Application.md) for subsystem lifetime and
[Scene and Actors](Scene.md) for runtime lifecycle.


## Implementation map

| Responsibility | Source |
| --- | --- |
| Shared subsystem declaration | `Engine/src/Physics/PhysicsSubsystem.h` |
| Jolt initialization and shutdown | `Engine/src/Physics/PhysicsSubsystem.cpp` |
| Runtime world declaration | `Engine/src/Physics/PhysicsWorld.h` |
| Body management and simulation | `Engine/src/Physics/PhysicsWorld.cpp` |
| Contact event representation | `Engine/src/Physics/PhysicsContactEvent.h` |
| Scene gravity settings | `Engine/src/Scene/ScenePhysicsSettings.h` |
| Rigid-body authoring data | `Engine/src/Scene/Components/RigidBodyComponent.h` |
| Box collider data | `Engine/src/Scene/Components/BoxColliderComponent.h` |
| Sphere collider data | `Engine/src/Scene/Components/SphereColliderComponent.h` |
| Capsule collider data | `Engine/src/Scene/Components/CapsuleColliderComponent.h` |
| Runtime world creation and updates | `Engine/src/Scene/Scene.cpp` |
| Lua contact-event dispatch | `Engine/src/Scripting/ScriptWorld.cpp` |

## Ownership

Application owns PhysicsSubsystem.

A Scene creates and owns PhysicsWorld when beginning Play or Simulate
runtime. Ending runtime or clearing the Scene releases that world.

PhysicsWorld borrows PhysicsSubsystem. The subsystem must outlive every
world that uses its allocator and job system.

| Owner | Owned infrastructure |
| --- | --- |
| PhysicsSubsystem | Jolt temporary allocator and job system |
| PhysicsWorld | PhysicsSystem, collision filters, contact listener, body bindings, and contact event storage |

PhysicsWorld receives the Scene registry by reference during body
management and updates. It does not own that registry.

Both subsystem and world explicitly disable copying and moving.

## Subsystem initialization

PhysicsSubsystem construction calls Initialize().

Initialization rejects an already-populated JPH::Factory::sInstance.
The implementation therefore does not support independently initializing
another subsystem while that global factory exists.

It performs these operations:

1. Registers Jolt's default allocator.
2. Installs logging callbacks.
3. Creates the global Jolt factory.
4. Registers Jolt types.
5. Creates a 10 MiB temporary allocator.
6. Creates the Jolt thread-pool job system.
7. Marks the subsystem initialized.

The job-system constructor receives -1 for its thread-count argument.
The reported maximum concurrency comes from the created job system;
Quiet does not hard-code the runtime log's concurrency value here.

Failures inside the initialization try block release the job system and
allocator, unregister types, delete the factory, clear callbacks, and rethrow.

## Runtime world initialization

PhysicsWorld construction requires an initialized subsystem.

It creates collision-layer interfaces, filters, and a PhysicsSystem with:

| Setting | Configured value |
| --- | --- |
| Maximum bodies | 65536 |
| Body mutex count argument | 0 |
| Maximum body pairs | 65536 |
| Maximum contact constraints | 10240 |

These are initialization parameters, not a guarantee that every requested
body or contact will be created successfully.

The world installs its contact listener and applies the supplied gravity.

If any gravity component is nonfinite, it logs a warning and uses
(0, -9.81, 0).

Scene passes its physics settings when constructing the world.
Editing those settings does not, by itself, reconstruct an existing world.

## Fixed-step simulation

PhysicsWorld::Update() first calls SynchronizeBodies() when its
PhysicsSystem exists.

Only afterward does it reject nonfinite or nonpositive deltaTime.
Such a call can therefore synchronize body membership without advancing
simulation time.

For a valid positive delta:

1. Adds deltaTime to the accumulator.
2. Caps accumulated time at four fixed steps.
3. Executes steps while enough accumulated time remains.
4. Synchronizes dynamic transforms back to the registry if any step ran.

| Timing setting | Value |
| --- | --- |
| Fixed step | 1 / 60 second |
| Accumulator cap | 4 / 60 second |
| Intended maximum catch-up steps per call | 4 |

The accumulator uses floating-point arithmetic. The cap bounds catch-up
work; it does not preserve all elapsed time after a long frame.

### Step ordering

Each fixed step:

1. Synchronizes kinematic bodies using the fixed timestep.
2. Calls Jolt PhysicsSystem::Update().
3. Logs a warning if Jolt reports an update error mask.
4. Subtracts the fixed timestep from the accumulator.

The Jolt call uses one collision step and the subsystem's temporary
allocator and job system.

An error mask does not automatically stop the loop or restore simulation
state.

### Frames without a step

When accumulated time is below the fixed timestep, no Jolt update runs.

Dynamic transform synchronization is performed only when at least one
step ran. This function does not interpolate dynamic transforms between
simulation steps.

### Long frames

A delta larger than the accumulator cap loses excess elapsed time.

For example, supplying 0.5 seconds does not request thirty simulation
steps. The accumulator is capped at approximately 0.0667 seconds.

This limits catch-up work and can make simulated time lag elapsed wall time.

### Application timing boundary

Application supplies its measured frame delta to Scene.
The physics world applies its own fixed-step accumulation.

The application loop and physics simulation therefore have different
timing contracts. A fixed physics timestep does not imply a fixed
rendering frame rate.

## Body eligibility

A runtime body requires:

- A valid registry entity.
- ActorIdentityComponent.
- TransformComponent.
- RigidBodyComponent.
- Exactly one supported collider.

Supported colliders are BoxCollider, SphereCollider, and CapsuleCollider.

CreateBodyBinding() rejects entities already represented by a binding.
It also rejects missing or multiple supported colliders.

BuildBodies() destroys existing bindings and attempts body creation for
entities with identity, transform, and rigid-body components.

Missing or multiple colliders produce warnings during this startup build.
The method returns void, so completion does not establish that every
candidate acquired a body.

A failed Jolt body allocation is logged and leaves that entity unbound.

## Collider construction

Collider geometry is derived from authored values and WorldTransform.Scale
when the body is created.

### Box

The effective full size is:

abs(collider.Size * transform.Scale)

Each axis is clamped to at least 0.001. Jolt receives half of that size
as the box half extents.

### Sphere

The effective radius uses the largest absolute scale axis.

A nonfinite authored radius falls back to 0.5.
The scaled radius is clamped to at least 0.001.

Nonuniform scale therefore produces a sphere based on the largest axis,
not an ellipsoid.

### Capsule

The capsule uses the larger absolute X/Z scale for radius and absolute
Y scale for height.

Nonfinite authored radius and height fall back to 0.5 and 2.0 respectively.

Effective radius is at least 0.001.
Effective total height is at least twice the effective radius plus 0.001.

Jolt receives half the remaining cylindrical height after subtracting
the two hemispherical radii.

### Collider center

All supported collider centers are multiplied by the signed transform scale.

When the squared effective center length exceeds 1e-12, the shape is wrapped
in a RotatedTranslatedShape with that offset and identity local rotation.

The body itself receives the Actor's position and rotation.

These conversions are not a general validation boundary for every nonfinite
transform, collider size, or center value.

## Rigid-body settings at creation

The body receives its motion type and initial pose from the authored
RigidBodyComponent and WorldTransform.

| Setting | Creation-time handling |
| --- | --- |
| Friction | Finite values clamped to at least zero; otherwise 0.2 |
| Restitution | Finite values clamped to 0 through 1; otherwise zero |
| Linear damping | Finite values clamped to at least zero; otherwise 0.05 |
| Angular damping | Finite values clamped to at least zero; otherwise 0.05 |
| Gravity factor | Finite value retained; otherwise 1 |
| Allow sleeping | Copied directly |

Static bodies use the NonMoving object layer.
Kinematic and Dynamic bodies use Moving.

Dynamic bodies request activation at creation.
Other motion types use DontActivate.

These are creation settings. Membership synchronization does not
automatically reapply them to existing bodies.

## Membership synchronization

SynchronizeBodies() removes bindings when:

- Their entity is no longer valid.
- Identity, Transform, or RigidBody is missing.
- The Actor UUID differs from the binding's recorded UUID.
- The supported collider count is no longer exactly one.

It then attempts creation for eligible entities that have no binding.
Invalid collider combinations are skipped without repeating startup
warnings every update.

Newly created bodies trigger broad-phase optimization.

### Existing-body changes

An existing binding is retained when its membership checks still pass.

The method does not compare collider dimensions, centers, transform scale,
friction, damping, gravity factor, or motion type against creation settings.

Replacing one collider type with another while retaining exactly one
supported collider does not itself trigger a rebuild.

Authored changes can therefore diverge from the existing Jolt body's shape
or settings until the body is recreated.

## Transform authority

| Authored motion type | Normal synchronization direction |
| --- | --- |
| Static | Initial Scene pose is supplied at body creation |
| Kinematic | Scene position and rotation are sent to Jolt before each fixed step |
| Dynamic | Jolt position and rotation are copied to Scene after stepping |

### Static bodies

These synchronization functions do not continuously push edited Static
transforms into Jolt.

Changing the Scene transform alone can leave the rendered Actor and its
existing physics body at different poses.

### Kinematic bodies

SyncKinematicBodies() calls MoveKinematic() using the Scene's position,
rotation, and fixed timestep.

It does not rebuild the collider from changed scale or dimensions.

### Dynamic bodies

SyncDynamicBodies() copies Jolt position and rotation into WorldTransform
after at least one fixed step.

Scale is left unchanged.

Assigning a Dynamic Actor's Scene position is not a physics teleport through
these functions. A later physics synchronization can overwrite it.

### Changing motion type

Synchronization direction is selected using the current authored
RigidBodyComponent.

The existing Jolt body's motion type was selected during creation.
SynchronizeBodies() does not update it.

Changing the component's motion type during runtime can therefore create
a mismatch between synchronization behavior and the actual physics body.

## Runtime commands

`Scene::ApplyPhysicsImpulse` and `Scene::SetPhysicsLinearVelocity`
resolve an Actor UUID and forward the command to the runtime physics
world. They return `false` when runtime physics is unavailable or the
Actor cannot be resolved.

`PhysicsWorld::ApplyImpulse` and `PhysicsWorld::SetLinearVelocity`
require a finite vector, an existing body binding, and a Dynamic Jolt
body. The motion-type check uses the actual physics body, rather than
the current `RigidBodyComponent` value.

A successful return means the command was issued. It does not mean the
Actor transform was immediately updated. Dynamic transforms are copied
back through the normal physics update.

## Collision filtering

Static bodies use the NonMoving object and broadphase layers.
Kinematic and Dynamic bodies use Moving.

| First layer | Second layer | Allowed by Quiet's filters |
| --- | --- | --- |
| NonMoving | NonMoving | No |
| NonMoving | Moving | Yes |
| Moving | Moving | Yes |

Filter admission does not guarantee a collision or contact callback.
Jolt's motion-type rules and the bodies' geometry still apply.

## Contact events

`PhysicsContactEvent` contains an event type and two Actor UUIDs.
The event types are Started and Ended. Events do not contain contact
positions, normals, or impulses, and there is no Stay event.

Jolt contact callbacks queue events in `PhysicsWorld`. The listener
checks Jolt's previous-contact state before queuing an event. Both
body IDs must resolve to bindings with valid Actor UUIDs.

The queue is protected by a mutex. `ConsumeContactEvents` drains it by
swapping out the buffered vector. Event order follows callback arrival;
the implementation does not sort events or canonicalize Actor pairs.

### Scene delivery

During `Scene::UpdateRuntime`, script updates run before the physics
update. The Scene then clears its previous contact-event buffer,
updates physics, and consumes the world's queued events.

When a ScriptWorld exists, the Scene dispatches those events after
physics returns. Lua collision callbacks therefore run during Scene
processing, rather than inside Jolt's contact callbacks.

Started events select `OnCollisionEnter`; Ended events select
`OnCollisionExit`. Each event is dispatched to Actor A with Actor B as
the argument, then to Actor B with Actor A as the argument.

Delivery requires a started, non-faulted script instance, the matching
optional callback, and a valid other Actor. A failed protected Lua
callback faults that script instance.

After contact dispatch, the Scene flushes queued Actor creations and
destructions, then synchronizes physics bodies again.

The Scene's contact-event buffer represents the most recent runtime
update. It is replaced on the next update and may be empty.

## Teardown

`DestroyBodies` removes and destroys the bound Jolt bodies, clears the
bindings, and clears queued contact events.

World destruction detaches the contact listener before destroying the
bodies and physics system. The PhysicsSubsystem must remain alive
through world teardown.

Consumers must not depend on receiving an Ended event for every
contact during body removal or shutdown.