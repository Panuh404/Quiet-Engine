# Quiet Documentation

Quiet's documentation explains how to build and use the engine, understand its implementation, and extend its systems.

The repository's main introduction remains in the root [README](../README.md). This directory is the entry point for the detailed handbook and documentation tooling.

## Documentation status

The documentation foundation includes conventions, local generation tooling, and C++ system groups. The generated reference currently provides an API inventory; detailed symbol contracts will be added incrementally.

The architecture overview and system chapters for Scene, Application,
Editor, Assets, Renderer, and Physics are available. Additional system
chapters are being added incrementally. Practical guides and file-format
references remain planned.

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
| [Scene and Actors](Systems/Scene.md) | Ownership, identity, components, copying, runtime lifecycle, deferred requests, and load/save behavior |
| [Application](Systems/Application.md) | Configuration, ownership, startup, frame orchestration, active Scene and camera selection, and shutdown |
| [Editor](Systems/Editor.md) | Responsibilities, ownership, commands, history, dirty state, Scene persistence, and runtime transitions |
| [Assets](Systems/Assets.md) | Identity, registry persistence, importing, ownership, incremental loading, replacement, and runtime loading |
| [Renderer](Systems/Renderer.md) | Ownership, frame synchronization, render passes, viewport resizing, swapchain recreation, and teardown |
| [Physics](Systems/Physics.md) | Ownership, fixed stepping, body and collider contracts, runtime commands, contact delivery, and teardown |
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
