# Quiet Documentation

Quiet's documentation explains how to build and use the engine, understand its implementation, and extend its systems.

The repository's main introduction remains in the root [README](../README.md). This directory is the entry point for the detailed handbook and documentation tooling.

## Documentation status

The documentation foundation is being established. Architecture chapters, practical guides, format references, and the generated C++ reference are planned; they are not available yet.

Current pages:

- [Documentation conventions](Contributing/Documentation.md): structure, writing standards, API comments, and verification requirements.

Existing branding assets remain in `Docs/Brand/`.

## Reading paths

As chapters become available, the handbook will support three reading paths:

| Goal | Suggested order |
| --- | --- |
| Build and use Quiet | Getting Started → Projects → Editor → Assets → Scripting → Cooking |
| Understand and extend Quiet | Architecture → Scene → Application → Editor → individual system chapters |
| Look up a specific contract | Generated C++ reference or File Formats reference |

Planned chapters are listed below as paths rather than links. Links will be added when their target pages exist.

## Planned handbook

### Foundations

| Proposed page | Scope |
| --- | --- |
| `Docs/GettingStarted.md` | Requirements, build presets, executable targets, resources, and first launch |
| `Docs/Architecture.md` | System boundaries, ownership, lifecycle, and major data flows |

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
| `Docs/Systems/Scene.md` | Actor identity, EnTT storage, components, scene cloning, serialization, and runtime mutation |
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

## Documentation milestones

| Milestone | Deliverable | State |
| --- | --- | --- |
| DOC-1 | Documentation index and conventions | Ready for review |
| DOC-2 | Optional Doxygen generation and theme | Planned |
| DOC-3 | Verified architecture overview | Planned |
| DOC-4 | Scene and Actor documentation pilot | Planned |
| DOC-5 | Remaining systems, one chapter at a time | Planned |
| DOC-6 | Practical guides | Planned |
| DOC-7 | File-format reference | Planned |
| DOC-8 | Root README rewrite and engine roadmap | Planned |
| DOC-9 | Complete documentation verification | Planned |
| DOC-10 | Optional automated generation and publishing | Optional follow-up |

DOC-5 proceeds through Application, Editor, Assets, Renderer, Physics, Audio, Scripting, and Cooking. Each system is reviewed and committed separately.

## Source and output layout

- The main `README.md` stays at the repository root.
- All other documentation source, configuration, and theme assets live under `Docs/`.
- Proposed generated output is `Build/Docs/`, already covered by the repository's build-output ignore rules.
- Documentation generation will be optional and independent of normal engine builds.
- Until DOC-2 is implemented, this index does not prescribe a documentation build command.

The handbook uses relative links so completed pages can be read directly in the repository. Doxygen publication will be checked separately for navigation and rendering compatibility.
