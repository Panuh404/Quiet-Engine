# Quiet Documentation

Quiet's documentation explains how to build and use the engine, understand its implementation, and extend its systems.

The repository's main introduction remains in the root [README](../README.md). This directory is the entry point for the detailed handbook and documentation tooling.

## Documentation status

The documentation foundation includes conventions, local generation tooling, and C++ system groups. The generated reference currently provides an API inventory; detailed symbol contracts will be added incrementally.

The architecture overview is available. Detailed system documentation is being
added incrementally, starting with Scene and Actor contracts. Practical guides
and file-format references remain planned.

Current pages:

- [Documentation conventions](Contributing/Documentation.md): structure, writing standards, API comments, and verification requirements.
- [Building the documentation](Contributing/BuildingDocumentation.md): prerequisites, theme initialization, generation, diagnostics, and troubleshooting.
- [Architecture overview](Architecture.md): workspace targets, ownership, scene/runtime separation, startup, execution flow, and packaging.
- [Scene and Actors](Systems/Scene.md): ownership, identity, component access, handle lifetime, Scene copying, and Actor duplication; runtime and serialization sections remain in progress.
Existing branding assets remain in `Docs/Brand/`.

## Reading paths

As chapters become available, the handbook will support three reading paths:

| Goal | Suggested order |
| --- | --- |
| Build and use Quiet | Getting Started → Projects → Editor → Assets → Scripting → Cooking |
| Understand and extend Quiet | Architecture → Scene → Application → Editor → individual system chapters |
| Look up a specific contract | Generated C++ reference or File Formats reference |

Available chapters are linked below. Planned chapters are shown as paths until their target pages exist.

## Handbook structure

### Foundations

| Proposed page | Scope |
| --- | --- |
| `Docs/GettingStarted.md` | Requirements, build presets, executable targets, resources, and first launch |
| [Architecture overview](Architecture.md) | System boundaries, ownership, lifecycle, and major data flows |

### Practical guides

| Proposed page | Scope |
| --- | --- |
| `Docs/Guides/Projects.md` | Launcher, project creation and opening, project layout, and Editor handoff |
| `Docs/Guides/Editor.md` | Panels, viewport navigation, scene editing, component authoring, and shortcuts |
| `Docs/Guides/Assets.md` | Importing, browsing, assigning, moving, and deleting assets |
| `Docs/Guides/Scripting.md` | Lua callbacks, exposed properties, script-facing bindings, and diagnostics |
| `Docs/Guides/Cooking.md` | Cooking an editable project and launching its runtime package |

### System internals

| Proposed page | Scope |
| --- | --- |
| [Scene and Actors](Systems/Scene.md) | Ownership, identity, components, handle lifetime, copying, and duplication; runtime lifecycle, deferred mutation, and serialization remain planned |
| `Docs/Systems/Application.md` | Application startup, project loading, frame orchestration, and shutdown |
| `Docs/Systems/Editor.md` | Controllers, commands, history, dirty state, and runtime transitions |
| `Docs/Systems/Assets.md` | Registry, importers, asset libraries, loading, and reference handling |
| `Docs/Systems/Renderer.md` | Vulkan resource ownership, frame flow, viewport targets, meshes, and selection passes |
| `Docs/Systems/Physics.md` | Jolt integration, fixed stepping, transform authority, colliders, and contacts |
| `Docs/Systems/Audio.md` | Subsystem and world ownership, sources, listeners, spatialization, and mixing |
| `Docs/Systems/Scripting.md` | Environments, lifecycle, bindings, metadata, hot reload, and deferred mutation |
| `Docs/Systems/Cooking.md` | Conversion stages, cache integrity, package construction, and runtime loading |

### Reference and direction

| Proposed page | Scope |
| --- | --- |
| `Docs/Reference/FileFormats.md` | Project and scene JSON, asset registry, runtime manifest, cook cache, and QAsset/QMesh/QTexture layouts |
| `Docs/Roadmap.md` | Engine direction, future work, and implemented milestones |

The C++ API reference will be generated from Quiet's source declarations and documentation comments. It will include relevant implementation headers, not only headers under `Engine/include/Quiet/`.


## Source and output layout

- The main `README.md` stays at the repository root.
- All other documentation source, configuration, and theme assets live under `Docs/`.
- Generated output is `Build/Docs/`, covered by the repository's build-output ignore rules.
- Documentation generation is optional and uses the standalone CMake project in `Docs/`.
- Generation commands and prerequisites are documented in [Building the documentation](Contributing/BuildingDocumentation.md).

The handbook uses relative links so completed pages can be read directly in the repository. Doxygen publication will be checked separately for navigation and rendering compatibility.
