# Quiet Documentation

Quiet's documentation explains how to build and use the engine, understand its implementation, and extend its systems.

The repository's main introduction remains in the root `README.md`. This directory is the entry point for the detailed handbook and documentation tooling.

## Documentation status

The documentation includes conventions, local generation tooling, and C++ system groups. The generated reference combines documented symbol contracts with a browsable API inventory; contract coverage is expanded incrementally.
The architecture overview, Getting Started, system chapters for Scene,
Application, Editor, Assets, Renderer, Physics, Audio, Scripting, and
Cooking, and practical guides for Projects, Editor, Assets, Scripting,
and Cooking are available. File-format references cover project and Scene JSON, runtime-package
metadata, and cooked binary assets.

Current pages:

- [Documentation conventions](Contributing/Documentation.md): structure, writing standards, API comments, and verification requirements.
- [Building the documentation](Contributing/BuildingDocumentation.md): prerequisites, theme initialization, generation, diagnostics, and troubleshooting.
- [Architecture overview](Architecture.md): workspace targets, ownership, scene/runtime separation, startup, execution flow, and packaging.
- [Scene and Actors](Systems/Scene.md): ownership, identity, component access, copying, runtime lifecycle, deferred Actor requests, and serialization contracts.
- [Application](Systems/Application.md): configuration, ownership, Editor and Sandbox startup, frame execution, Scene and camera selection, and shutdown.
- [Editor](Systems/Editor.md): responsibilities, ownership, requests, mutation boundaries, history, dirty state, Scene persistence, and runtime transitions.
- [Assets](Systems/Assets.md): identity, registry persistence, importing, library ownership, incremental loading, replacement, and runtime package loading.
- [Renderer](Systems/Renderer.md): ownership, synchronization, render passes, viewport resizing, swapchain recreation, and teardown.
- [Physics](Systems/Physics.md): ownership, fixed stepping, body construction, transform synchronization, runtime commands, collision filtering, and contact delivery.
- [Audio](Systems/Audio.md): ownership, sound construction, runtime synchronization, playback commands, listeners, spatialization, mixing, one-shots, previews, and teardown.
- [Scripting](Systems/Scripting.md): Lua ownership, environments, callbacks, synchronization, hot reload, properties, bindings, deferred Actor requests, and shutdown.
- [Cooking](Systems/Cooking.md): pipeline ownership, caching, conversion, native packaging, writers, validation, cleanup, and runtime package consumption.
- [Getting Started](GettingStarted.md): prerequisites, presets, configuration, building, resources, first launch, and troubleshooting.
- [Projects](Guides/Projects.md): Launcher creation and opening, project locations and layout, direct Editor launch, and troubleshooting.
- [Editor guide](Guides/Editor.md): panels, Actor editing, viewport navigation, gizmos, history, Scene persistence, and runtime testing.
- [Assets guide](Guides/Assets.md): importing, inspection, assignment, audio previews, organization, registry removal, and file deletion.
- [Scripting guide](Guides/Scripting.md): Lua assignment, callbacks, exposed properties, runtime APIs, deferred creation, errors, and hot reload.
- [Cooking guide](Guides/Cooking.md): project preparation, Cooker builds and execution, package contents, Sandbox launch, caching, and troubleshooting.
- [File formats](Reference/FileFormats.md): navigation for persisted-data references.
- [Project file formats](Reference/ProjectFiles.md): project descriptors, asset registries, and user project history.
- [Scene file format](Reference/SceneFiles.md): settings, Actor identity, component fields, hierarchy, defaults, and persistence boundaries.
- [Runtime package formats](Reference/RuntimePackageFiles.md): manifest fields, path validation, cache entries, fingerprints, signatures, and persistence behavior.
- [Binary asset formats](Reference/BinaryAssets.md): container headers, mesh payloads and codecs, texture payloads, byte offsets, and reader validation.
- [Engine roadmap](Roadmap.md): implemented foundations, future development areas, and development principles.

Existing branding assets remain in `Docs/Brand/`.

## Reading paths

Use the following reading paths:

| Goal | Suggested order |
| --- | --- |
| Build and use Quiet | Getting Started → Projects → Editor → Assets → Scripting → Cooking |
| Understand and extend Quiet | Architecture → Scene → Application → Editor → individual system chapters |
| Look up a specific contract | Generated C++ reference or File Formats reference |

The tables below link to the available handbook chapters.

## Handbook structure

### Foundations

| Page | Scope |
| --- | --- |
| [Getting Started](GettingStarted.md) | Prerequisites, configuration, builds, resources, first launch, and troubleshooting |
| [Architecture overview](Architecture.md) | System boundaries, ownership, lifecycle, and major data flows |

### Practical guides

| Page | Scope |
| --- | --- |
| [Projects](Guides/Projects.md) | Launcher creation and opening, project layout, direct Editor launch, and handoff |
| [Editor](Guides/Editor.md) | Panels, Actor components, navigation, gizmos, shortcuts, saving, and Play/Simulate |
| [Assets](Guides/Assets.md) | Importing, inspection, assignment, previews, file organization, and removal |
| [Scripting](Guides/Scripting.md) | Lua examples, assignment, properties, bindings, runtime testing, and diagnostics |
| [Cooking](Guides/Cooking.md) | Project preparation, package production, Sandbox launch, caching, and diagnostics |

### System internals

| Page | Scope |
| --- | --- |
| [Scene and Actors](Systems/Scene.md) | Ownership, identity, components, copying, runtime lifecycle, deferred requests, and load/save behavior |
| [Application](Systems/Application.md) | Configuration, ownership, startup, frame orchestration, active Scene and camera selection, and shutdown |
| [Editor](Systems/Editor.md) | Responsibilities, ownership, commands, history, dirty state, Scene persistence, and runtime transitions |
| [Assets](Systems/Assets.md) | Identity, registry persistence, importing, ownership, incremental loading, replacement, and runtime loading |
| [Renderer](Systems/Renderer.md) | Ownership, frame synchronization, render passes, viewport resizing, swapchain recreation, and teardown |
| [Physics](Systems/Physics.md) | Ownership, fixed stepping, body and collider contracts, runtime commands, contact delivery, and teardown |
| [Audio](Systems/Audio.md) | Ownership, runtime sources, playback contracts, listeners, spatial updates, shared mixing, previews, and teardown |
| [Scripting](Systems/Scripting.md) | Lua lifecycle, environments, callbacks, synchronization, reload, property discovery, bindings, and deferred mutation |
| [Cooking](Systems/Cooking.md) | Pipeline, cache integrity, asset conversion and packaging, writer contracts, cleanup, and runtime consumption |

### Reference and direction

| Page | Scope |
| --- | --- |
| [File formats](Reference/FileFormats.md) | Project and Scene JSON, runtime manifest, cook cache, QAsset, QMesh, and QTexture |
| [Engine roadmap](Roadmap.md) | Implemented foundations, future areas, and development principles |

The C++ API reference is generated from Quiet's source declarations and documentation comments. It includes public headers under `Engine/include/Quiet/` and relevant implementation headers.

## Source and output layout

- The main `README.md` stays at the repository root.
- All other documentation source, configuration, and theme assets live under `Docs/`.
- Generated output is `Build/Docs/`, covered by the repository's build-output ignore rules.
- Documentation generation is optional and uses the standalone CMake project in `Docs/`.
- Generation commands and prerequisites are documented in [Building the documentation](Contributing/BuildingDocumentation.md).

The handbook uses relative links so pages can be read directly in the
repository. The [published documentation](https://panuh404.github.io/Quiet-Engine/)
combines the handbook with the generated C++ reference. GitHub Actions
validates documentation changes and publishes successful builds from main.