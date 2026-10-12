# Engine Roadmap

Quiet is developed incrementally, with emphasis on clear ownership,
Editor/runtime separation, and practical workflows.

This roadmap distinguishes implemented foundations from future areas.
It is not a release schedule or a promise that every future area has
a finalized design.

## Implemented foundations

| Area | Current foundation | Details |
| --- | --- | --- |
| Application and projects | Executable modes, Launcher handoff, staged Editor startup, project descriptors, and registries | [Application](Systems/Application.md), [Projects](Guides/Projects.md) |
| Scene and ECS | EnTT-backed identity and components, Scene cloning, persistence, and deferred runtime requests | [Scene and Actors](Systems/Scene.md) |
| Editor | Docked tools, hierarchy organization, Inspector, history, dirty state, and runtime transitions | [Editor](Systems/Editor.md), [Editor guide](Guides/Editor.md) |
| Viewport and rendering | Vulkan static meshes, frame synchronization, viewport targets, grid, picking, gizmos, navigation, and selection outlines | [Renderer](Systems/Renderer.md) |
| Assets | Registry identity, importing, loading, assignment, previews, and physical project-file operations | [Assets](Systems/Assets.md), [Assets guide](Guides/Assets.md) |
| Physics | Fixed stepping, primitive colliders, rigid bodies, transform synchronization, and contact delivery | [Physics](Systems/Physics.md) |
| Audio | Runtime sources/listeners, spatial playback, commands, shared mixing, and previews | [Audio](Systems/Audio.md) |
| Scripting | Lua environments, callbacks, bindings, exposed properties, synchronization, and hot reload | [Scripting](Systems/Scripting.md) |
| Cooking | Mesh/texture conversion, native audio/script packaging, cache integrity, manifests, and runtime consumption | [Cooking](Systems/Cooking.md), [Cooking guide](Guides/Cooking.md) |

Implemented foundation does not imply completion of an entire domain.
The linked chapters record the current contracts and limits.

## Future development areas

These areas preserve the direction established in the existing roadmap.

### Text and runtime UI

Develop text rendering and user-interface systems for runtime content,
building beyond the current Dear ImGui Editor tooling.

### Navigation

Introduce navigation through Recast and Detour.
Define authoring, generation, runtime ownership, and gameplay-facing
interfaces as part of that work.

### Gameplay and physics expansion

Extend the current runtime foundation with additional gameplay and
physics systems. Establish each feature's component, lifecycle, and
scripting contracts before treating it as complete.

### Advanced acoustics

Explore Steam Audio integration for environmental acoustics beyond
the current miniaudio source/listener foundation.

### Rendering expansion

Extend runtime rendering beyond the current static-mesh foundation
while keeping resource lifetime, synchronization, and format requirements
explicit.

### Build and distribution

Develop distribution workflows around the existing Cooker and Sandbox.
Executable packaging, runtime dependencies, and deployment validation
remain separate work from producing a content package.

## Development principles

- Keep persistent authoring data separate from backend runtime objects.
- Preserve the editable Scene when testing runtime behavior.
- State ownership, lifetime, validation, and failure boundaries explicitly.
- Build features in reviewable increments.
- Validate complete workflows as well as individual operations.
- Update documentation when a completed feature changes its contracts.

## Current boundaries to carry forward

- Hierarchy folders do not provide transform parenting.
- Existing physics bindings do not automatically refresh every authored setting.
- Existing audio sounds retain most creation-time settings.
- Hot reload does not migrate arbitrary Lua state or roll back all side effects.
- Cooked textures use BC7 sRGB.
- Asset packaging does not produce a complete executable distribution.

These boundaries describe the current implementation. They do not
assign additional roadmap commitments by themselves.