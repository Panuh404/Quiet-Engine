# Architecture Overview

Quiet is a C++20 engine and editor using Vulkan for rendering. Its workspace contains a shared engine library and separate executables for project management, editing, standalone runtime execution, and asset cooking.

This chapter introduces the current implementation boundaries, ownership model, and execution flow. Detailed system chapters expand these topics with API contracts and worked examples.

## Workspace targets

| Target | Role | Entry point or definition |
| --- | --- | --- |
| `Quiet_Engine` | Shared static library containing engine systems and editor integration | `Engine/CMakeLists.txt` |
| `Quiet_Launcher` | Project discovery, creation, and Editor process handoff | `Launcher/src/main.cpp` |
| `Quiet_Editor` | Interactive scene editing and runtime testing | `Editor/src/main.cpp` |
| `Quiet_Sandbox` | Standalone runtime execution | `Sandbox/src/main.cpp` |
| `Quiet_Cooker` | Asset conversion and runtime-package construction | `Cooker/src/main.cpp` |

`Quiet::Engine` is the CMake alias for `Quiet_Engine`. All four executables link this library.

Executable separation provides distinct entry points and workflows. The shared library currently also contains editor controllers, editor UI, and editor rendering support. These responsibilities are not separate engine/editor library targets.

## Source organization

`Engine/include/Quiet/` contains headers exposed by the engine target.

`Engine/src/` contains implementations and internal headers. Several central types, including `Quiet::Scene::Scene` and `Quiet::Scene::Actor`, are declared in this internal tree.

A header's location therefore matters when understanding the current build interface. It does not determine whether the type is important to the architecture.

Principal implementation locations:

| Responsibility | Source location | Principal symbol |
| --- | --- | --- |
| Application orchestration | `Engine/src/Core/Application.cpp` | `Quiet::Core::Application` |
| Application configuration | `Engine/include/Quiet/Core/AppSpec.h` | `Quiet::Core::AppSpec` |
| Project state and paths | `Engine/include/Quiet/Project/ProjectContext.h` | `Quiet::Project::ProjectContext` |
| Project-opening workflow | `Engine/src/Project/ProjectWorkflow.cpp` | `Quiet::Project::ProjectWorkflow` |
| Editable asset loading | `Engine/src/Project/ProjectAssetLoader.cpp` | `Quiet::Project::ProjectAssetLoader` |
| Packaged asset loading | `Engine/src/Project/RuntimeAssetLoader.cpp` | `Quiet::Project::RuntimeAssetLoader` |
| Scene storage and runtime | `Engine/src/Scene/Scene.cpp` | `Quiet::Scene::Scene` |
| Actor component access | `Engine/src/Scene/Actor.h` | `Quiet::Scene::Actor` |
| Scene persistence | `Engine/src/Scene/SceneSerializer.cpp` | `Quiet::Scene::SceneSerializer` |
| Editor state and operations | `Engine/include/Quiet/Editor/EditorController.h` | `Quiet::Editor::EditorController` |
| Scene history | `Engine/include/Quiet/Editor/EditorSceneHistory.h` | `Quiet::Editor::EditorSceneHistory` |
| Runtime transitions | `Engine/src/Editor/EditorRuntimeController.cpp` | `Quiet::Editor::EditorRuntimeController` |
| Renderer orchestration | `Engine/src/Renderer/Vulkan/VulkanRenderer.h` | `Quiet::Renderer::Vulkan::VulkanRenderer` |
| Physics simulation | `Engine/src/Physics/PhysicsWorld.cpp` | `Quiet::Physics::PhysicsWorld` |
| Runtime audio | `Engine/src/Audio/AudioWorld.h` | `Quiet::Audio::AudioWorld` |
| Runtime scripting | `Engine/src/Scripting/ScriptWorld.cpp` | `Quiet::Scripting::ScriptWorld` |
| Launcher handoff | `Launcher/src/LauncherApplication.cpp` | `Quiet::Launcher::LauncherApplication::LaunchEditor` |
| Package finalization | `Cooker/src/Package/PackageBuilder.cpp` | `Quiet::Cooker::FinalizeRuntimePackage` |

## Ownership and lifetime

Ownership determines how long state and resources remain available and which object is responsible for their destruction.

For Editor and Sandbox, `Quiet::Core::Application` is the main composition point. It constructs the required systems and connects their dependencies.

| Owner | Owned state or resources |
| --- | --- |
| `Application` | Project context and project registry |
| `Application` | Window, input, free camera, renderer, physics subsystem, and audio subsystem |
| `Application` | Project workflow and editable asset loader |
| `Application` | Main scene and optional editor runtime scene |
| `Application`, in Editor mode | Editor controller, scene workflow, and runtime controller |
| `VulkanRenderer` | Vulkan rendering infrastructure, mesh and texture libraries, render systems, and ImGui layer |
| `AudioSubsystem` | miniaudio engine, audio library, preview sound, and audio mixing groups |
| `PhysicsSubsystem` | Jolt initialization resources, temporary allocator, and job system |
| `Scene` | EnTT registry, Actor handles, hierarchy metadata, scene settings, and runtime request queues |
| Active runtime `Scene` | Physics world, audio world, and script world |
| `EditorController` | Editor state and scene history |

Several objects receive references to resources owned elsewhere. For example, `ProjectAssetLoader` uses renderer-owned mesh and texture libraries and the audio subsystem's library.

`EditorRuntimeController` receives a reference to the application's runtime-scene pointer. It creates and destroys that scene through the pointer, while `Application` contains the owning pointer itself.

Subsystem and world lifetimes serve different purposes:

- A subsystem provides application-level backend resources.
- A world provides state associated with an active scene runtime.

Stopping a runtime scene destroys its worlds while the application-level subsystems remain available.

## Application entry points

### Editor and Sandbox

Both executables construct an `AppSpec` and pass it to `Application`.

`AppSpec::Mode` selects Editor or Sandbox behavior. The specification also carries window settings, an optional editable project path, and an optional runtime package path.

The application constructor initializes common systems first, then performs mode-specific startup.

`Application::Run()` polls events and dispatches Editor or Sandbox frames until the application exits.

### Launcher

The Launcher uses its own `LauncherApplication` and `LauncherRenderer`.

It manages project discovery and creation, then launches the Editor as a separate process. `LaunchEditor()` resolves the Editor executable beside the Launcher executable and passes the selected project's absolute path as an argument.

After a successful handoff, the Launcher requests its own exit.

### Cooker

The Cooker has a command-line workflow in `Cooker/src/main.cpp`.

It reads an editable project descriptor and asset registry, processes registered assets, and constructs a runtime package. It does not create the Editor/Sandbox `Application` or run its frame loop.

## Projects and assets

Quiet distinguishes three kinds of project-related state:

| State | Purpose |
| --- | --- |
| Project registry | Tracks projects available to the Launcher and project workflows |
| Project descriptor | Describes an editable project, its directories, and startup scene |
| Asset registry | Maps asset IDs to types, paths, and built-in status |

`ProjectContext` holds the active project's descriptor, paths, and asset registry.

Asset IDs and file paths have separate responsibilities. Components refer to asset IDs; loading resolves those IDs through the active project context and the appropriate asset library.

The principal asset loading paths are:

- `ProjectAssetLoader` loads editable source assets and built-in support assets.
- `RuntimeAssetLoader` loads cooked mesh, texture, and audio assets described by a runtime manifest.
- Script source is resolved by the scripting system through the project context.

The registry describes assets. The libraries hold loaded representations used by the engine.

Mesh and texture libraries are owned by the renderer. The audio library is owned by the audio subsystem.

## Scene and Actor model

A `Scene` owns an EnTT registry containing entity components.

An `Actor` is a handle connecting component operations to an entity in that registry. It does not independently own the entity's component storage.

Scene-created Actors have identity and Transform components. Optional components describe rendering, cameras, lighting, rotation, physics, audio, and scripting.

Actor identity uses a Quiet UUID. EnTT entity identifiers belong to a particular registry and are not the persistent cross-scene identity mechanism.

The Scene also owns a vector of Actor handles. References to elements of that vector must be evaluated against operations that can move or erase its elements.

Editor hierarchy metadata records folders and ordering. These folders organize the editor presentation; they do not establish parent-child transform relationships.

### Authored data and resolved resources

Component values describe the scene's authored state. Backend simulation and playback objects belong to runtime worlds.

Rendering includes a resolved-resource connection: `StaticMeshComponent` stores both a `MeshResourceID` and a shared pointer to a loaded `StaticMesh`.

Consequently, scene data and loaded resource ownership should be considered separately.

Scene serialization writes the scene's persistent representation. It does not serialize active Jolt bodies, audio playback objects, or Lua execution environments.

## Editor and runtime scenes

The Editor supports Edit, Simulate, and Play modes.

| Mode | Active scene | Behavior |
| --- | --- | --- |
| Edit | Main editor scene | Authoring and editor interaction |
| Simulate | Runtime copy | Scene runtime with editor-oriented viewing |
| Play | Runtime copy | Scene runtime with play-oriented input and camera behavior |

Entering Simulate or Play is handled by `EditorRuntimeController`.

The transition:

1. Stops any previous runtime scene.
2. Stops editor audio preview.
3. Copies the editor scene into a new `Scene`.
4. Starts that copy's runtime.
5. Switches the editor mode.

Play additionally enables mouse capture and records the previous capture state for restoration.

The Scene copy creates its own registry and Actor handles, preserves Actor UUIDs, and copies the supported component values, hierarchy, and scene settings.

Physics, audio, and script worlds are created when runtime begins. They are not copied from another scene.

Copying `StaticMeshComponent` also copies its shared mesh pointer. This means the scene's component storage is separate while resolved mesh resources can remain shared.

Stopping runtime ends its worlds and destroys the runtime copy. Runtime component changes are not applied back to the editor scene by this transition.

## Editor startup

Editor project startup is staged through `Application::EditorBootstrapStage`:

1. `WaitingForFirstFrame`
2. `PreparingProject`
3. `LoadingAssets`
4. `LoadingScene`
5. `Ready`

`Failed` records a bootstrap failure and its message.

Bootstrap advancement occurs after a completed loading-screen frame. Asset loading advances incrementally through the editable asset loader.

This is work distributed across bootstrap frames. The staged workflow itself does not establish an asynchronous background asset-loading system.

Editable project startup can continue with diagnostics for assets that fail to load. If its startup scene cannot be loaded, the project workflow creates a default scene.

## Frame and runtime execution

The common frame start polls window events, updates input, processes global shortcuts, calculates elapsed time, and handles pending viewport resizing.

Once Editor bootstrap is ready, the Editor frame processes editor requests, updates the active scene, builds UI, resolves the scene to render, and calls the renderer.

The scene is resolved again for rendering because requests processed during a frame can affect which scene is active.

Sandbox updates its runtime scene, builds its runtime presentation, and renders without the editor grid or selected-Actor feedback.

### Scene runtime order

`Scene::Update()` performs simulation only while runtime is active.

`Scene::UpdateRuntime()` currently executes in this order:

1. Update eligible Rotator components.
2. Update scripts.
3. Step physics and collect contact events.
4. Dispatch physics contact events to scripts.
5. Flush deferred Actor creation requests.
6. Flush deferred Actor destruction requests.
7. Synchronize physics bodies with the resulting component storage.
8. Update audio sources and the active listener.

This order is part of the system integration. For example, collision callbacks occur after physics, and deferred structural changes are applied after those callbacks.

Physics uses a fixed step of `1 / 60` seconds with accumulation bounded to four steps per frame. Script updates receive the frame's elapsed time; their update cadence is not the same fixed-step loop.

Both Simulate and Play create physics, audio, and script worlds through `Scene::BeginRuntime()`.

## Cooking and standalone packages

The Cooker converts the editable project into runtime content:

| Asset type | Runtime representation |
| --- | --- |
| Mesh | Optimized and encoded `.qmesh`, with optional Zstandard compression |
| Texture | `.qtexture` containing GPU-ready BC7 data stored in KTX2 |
| Audio | Native WAV, FLAC, or MP3 content |
| Script | Lua source content |

Package construction also includes the startup scene and `RuntimeManifest.json`.

The manifest maps asset IDs to package-relative paths. The incremental cook cache supports reuse of validated outputs; the manifest describes the package consumed by runtime.

When Sandbox receives a runtime package path, it:

1. Validates and reads the manifest.
2. Initializes a runtime project context from it.
3. Loads built-in support assets.
4. Loads packaged mesh, texture, and audio assets.
5. Loads the packaged startup scene.
6. Starts Play runtime on the loaded scene.

This path does not fall back to editable project assets when package startup fails.

When no package path is supplied, Sandbox uses the last-opened or default editable-project workflow instead.

A runtime package makes project content independent of editable source files. Sandbox still uses the executable and engine resources supplied by the application build.

## Editor state and history

`EditorController` keeps editor session state separate from the scene's component storage.

`EditorSceneHistory` captures scene snapshots containing supported component values, hierarchy metadata, scene settings, and selection identity.

Editor mutations use before-and-after snapshots for undo/redo. Dirty state is evaluated against a saved scene snapshot.

History capture and scene copying explicitly enumerate supported components. Adding a new component therefore requires reviewing more than its declaration.

## Shutdown

During normal shutdown, `Application::ShutdownCommon()` first stops editor runtime and any active main-scene runtime.

It then releases workflow/controller objects, scenes, audio and physics subsystems, the free camera, renderer, input, and window.

Runtime worlds borrow subsystem resources, so their destruction must precede subsystem destruction.

Scenes and editor history can retain shared mesh resources. The current application shutdown releases those owners before destroying the renderer.

These lifetime relationships must be preserved when ownership or shutdown behavior changes.

## Extending the implementation

Use the current boundaries when deciding where a change belongs:

- Scene and component code describes Actor state and scene behavior.
- Runtime worlds own backend simulation, playback, and script execution state.
- Application code connects system lifetimes and coordinates execution.
- Editor controllers and workflows manage authoring operations and transitions.
- Renderer code manages GPU resources and rendering.
- Project loading resolves editable project assets.
- Cooker and runtime readers implement the packaged asset path.

For a new persistent component, inspect scene copying, duplication, serialization, history capture, Inspector authoring, and any runtime-world integration that applies.

For a new asset type, inspect registration, editable loading, assignment/reference handling, cooking, manifest representation, and runtime loading.

These are review paths rather than a universal implementation checklist. The required changes depend on the component or asset's responsibilities.

Further chapters will document these systems individually. Planned chapters remain listed in the [documentation index](README.md).