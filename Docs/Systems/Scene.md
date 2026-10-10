# Scene and Actors

This chapter explains Scene storage, Actor identity, component access, and
handle lifetime. Cloning, runtime transitions, deferred mutation, and
serialization will be documented in the next increment.

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

## Worked example

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

## Current limitations and extension points

Actor lookup currently searches the handle vector linearly.

Explicit-ID creation relies on the caller to supply a valid, unused UUID.
Validation would belong in `Scene::CreateActorWithID()` if this policy changes.

Direct component access modifies registry data. These templates do not
themselves record editor history or explicitly synchronize runtime worlds.
Those integrations require separate documentation.

The next increment will cover Scene copying, Actor duplication, runtime
lifecycle, deferred Actor requests, and serialization.