<p align="center">
  <img src="Docs/Brand/quiet-logo.png" alt="Quiet Engine" width="512">
</p>

<h1 align="center">QUIET</h1>

Quiet is a C++20 Vulkan engine project with an integrated editor workflow.

The current focus is an editor-first engine foundation: scene editing, project management, asset registration, viewport tooling, runtime mode switching, serialization, undo/redo, and a modern Dear ImGui-based editor UI.

Quiet is in active prototype development. It is not a production engine yet. The goal is to build a stable, understandable engine foundation one system at a time.

---

## Current Status

Implemented foundations:

* Vulkan renderer with swapchain, depth resources, frame synchronization, scene viewport render target, and static mesh rendering
* EnTT-backed ECS foundation with registry-owned actor identity and components, lightweight Actor handles, ECS runtime iteration, and EnTT-driven static mesh rendering
* Jolt Physics runtime foundation with fixed-step simulation, Static / Dynamic / Kinematic rigid bodies, primitive colliders, scene gravity, contact events, and runtime body controls
* miniaudio runtime foundation with WAV / FLAC / MP3 assets, editor preview, Audio Source / Listener components, runtime playback controls, 3D spatial audio, attenuation, directional cones, Doppler, runtime source lifecycle synchronization, and Master / Music / SFX / UI mixing
* Lua 5.4 + sol2 scripting foundation with Script assets, persistent Script components, per-Actor isolated environments, lifecycle and collision callbacks, engine bindings, deferred runtime Actor mutation, exposed Inspector properties, runtime hot reload, and editor source diagnostics
* Dear ImGui dockspace editor shell
* dark professional editor theme
* high-DPI aware editor UI scaling
* scene viewport with an adaptive world grid, actor picking, hover/selection feedback, Fly/Orbit/Pan navigation, mesh-placement previews, and ImGuizmo editing
* standalone Quiet Launcher for project discovery, creation, and Editor handoff
* staged Editor project bootstrap with visible incremental mesh, texture, and audio loading
* scene save/load workflow
* dual-source Asset Browser with editable physical Project content, read-only Engine resources, and a project-local asset registry
* mesh and texture import/loading foundation
* editor hierarchy with folders, drag/drop ordering, and persistent hierarchy metadata
* dedicated Scene Settings panel for scene-wide rendering, physics, and audio mix controls
* inspector with validated component editing, reset, copy/paste, undo/redo, and dirty-state integration
* undo/redo and dirty-state tracking
* Edit / Simulate / Play runtime modes with isolated runtime scene copies and Jolt physics simulation
* console and diagnostics panels
* editor branding with application icon, window icon, README logo, and topbar logo

---

## Workspace Targets

The workspace currently contains four main targets:

```text
Quiet_Engine     - core static engine library
Quiet_Launcher   - project launcher executable
Quiet_Editor     - editor executable
Quiet_Sandbox    - runtime sandbox executable
```

The editor target is currently the primary development target.

---

## Editor Features

### Editor Shell

* Dear ImGui docking layout
* dark professional editor style
* high-DPI scaling support
* top toolbar with Quiet branding
* resettable default layout
* project manager modal
* docked panels:

  * Viewport
  * Scene / Hierarchy
  * Inspector
  * Scene Settings
  * Assets
  * Console
  * Debug

### Viewport

* offscreen scene viewport render target
* procedural GPU world grid on the world XZ plane
* adaptive grid scale with smooth 1/10, 10/100, and 100/1000 transitions
* distance and horizon fading for stable large-scene navigation
* world X and Z axis visualization
* Edit and Simulate grid rendering with automatic Play/Sandbox suppression
* CPU actor picking for Static Mesh actors and editor debug icons
* projected mesh bounds and actor names for hover feedback
* depth-aware GPU silhouette highlighting for selected visible Static Mesh actors
* selected actor origin markers and names
* Camera and Directional Light editor icons and overlays
* configurable Viewport display options with selection-feedback control
* editor-only Box, Sphere, and Capsule collider wireframe visualization
* collider visualization matched to runtime physics scaling and local Center offsets
* motion-type visualization for Static, Dynamic, and Kinematic rigid bodies
* formal cursor capture with focus-loss cancellation and cursor restoration
* RMB Fly navigation
* Alt+LMB Orbit navigation around selected bounds or a resolved scene pivot
* MMB camera-plane Pan navigation
* configurable logarithmic camera speed with presets
* RMB-wheel speed adjustment and Shift speed boost
* shared navigation controls for free and piloted Camera actors
* contextual idle, Fly, Orbit, Pan, and Pilot navigation guides
* live Viewport mode and navigation status overlay
* viewport overlay toolbar
* ImGuizmo integration
* translate / rotate / scale tools
* local/world gizmo mode
* snapping toggle through the Viewport toolbar and G shortcut
* editable per-axis translation snapping increments
* editable rotation and scale snapping increments
* bounded snapping values with default-increment restoration
* focus selected actor
* drag mesh assets from the Asset Browser into the Viewport
* live world-space mesh placement preview using source bounds
* cursor-ray actor placement with ground-plane fallback
* lowest-bound ground alignment for dropped meshes
* undoable Static Mesh actor creation from asset drops

### Hierarchy

* persistent editor-only hierarchy metadata
* actor rows
* folder rows
* create folder
* rename folder
* delete empty folder
* drag actors into folders
* move actors back to root
* reorder root actors and folders
* drag/drop root ordering
* hierarchy validation and sanitization on load
* undo/redo integration
* save/reload persistence

Folders are editor organization nodes only. They do not change runtime transforms or create parent-child transform relationships.

### Inspector

* actor header and rename workflow
* Transform editing
* Static Mesh resource and visibility editing
* Material editing with fallback material controls
* imported material-slot overrides with source names and submesh usage
* compact material-slot texture previews
* Directional Light component editing
* Rotator component editing
* Camera component editing and piloting controls
* Audio Source component editing with Audio asset assignment, bus routing, volume, pitch, Play On Start, Loop, and 2D / 3D spatialization
* spatial Audio Source controls for distance attenuation, directional cones, and Doppler
* Audio Listener component editing with Primary-listener selection
* Script component editing with Script asset assignment and source-aware component headers
* exposed Lua property editing for Float, Int, Bool, String, Vector2/3/4, Color3/4, Actor references, and Mesh / Texture / Audio / Script asset references
* automatic exposed-property reconciliation when Lua source declarations change
* Script source diagnostics with Open Source, Reveal Source, and manual metadata refresh actions
* Rigid Body component editing for Static, Dynamic, and Kinematic motion
* rigid-body friction, restitution, damping, gravity-factor, and sleeping controls
* Box Collider authoring with Size and Center
* Sphere Collider authoring with Radius and Center
* Capsule Collider authoring with Radius, total Height, and Center
* one-collider-per-Actor authoring validation
* Add Component menu
* consistent component action menus
* Reset Component support across Transform, rendering, Camera, Audio Source, Audio Listener, Script, Rigid Body, and collider components
* typed component-value clipboard for Transform, rendering, Camera, Audio Source, Audio Listener, Script, Rigid Body, and collider components
* cross-actor Copy Component and Paste Component Values workflows
* asset, slot-count, and component-type validation for pasted values
* contextual explanations for unavailable Paste actions
* atomic undo/redo and dirty-state integration for component actions and value editing
* bounded numeric validation for Transform scale, lighting, material colors, and Camera projection values
* searchable Mesh, Texture, Audio, and Script asset pickers
* asset search by registry ID and path
* texture previews and source diagnostics in picker results
* drag/drop Mesh and Texture assignment
* Audio asset metadata for encoding, channels, sample rate, frame count, duration when available, and file size
* WAV / FLAC / MP3 audio import
* persistent editor Audio preview with Preview / Stop controls

### Scene Settings

Scene-wide rendering, physics, and audio mix settings live in a dedicated panel instead of the actor Inspector.

Current controls:

* Master audio volume
* Music bus volume
* SFX bus volume
* UI bus volume
* reset audio mix
* directional scene light direction
* directional scene light intensity
* ambient color
* ambient intensity
* scene gravity vector
* normalize light direction
* reset directional lighting
* reset ambient
* reset physics
* reset all scene settings

Scene Settings are serialized with the scene and are tracked by dirty-state and undo/redo.

### Scripting

* Lua 5.4 runtime with sol2 integration
* one isolated Lua environment per scripted Actor
* persistent Script component with project Script asset references
* runtime `OnCreate`, `OnUpdate`, and `OnDestroy` callbacks
* post-physics `OnCollisionEnter` and `OnCollisionExit` callbacks
* protected callback execution with per-instance error isolation
* automatic runtime Script instance synchronization
* runtime hot reload that preserves the previous working instance when replacement source fails
* Actor identity and Transform bindings
* Dynamic Rigid Body impulse and linear-velocity bindings
* Audio Source playback bindings
* Keyboard and mouse input bindings
* Scene Actor lookup by ID and name
* safe UUID-backed Actor references
* deferred runtime Actor creation, duplication, and destruction
* Script-exposed Float, Int, Bool, String, Vector2, Vector3, Vector4, Color3, Color4, Actor, Mesh, Texture, Audio, and Script properties
* serialized exposed-property overrides with undo/redo, copy/paste, Scene cloning, and runtime duplication support
* automatic editor-side Lua source monitoring and exposed-property reconciliation
* project-wide Script source validation with Valid / Error asset states
* Script source opening from the Inspector, Asset Browser context menu, and Asset Browser double-click

### Assets

* project-local asset registry
* Mesh, Texture, Audio, and Script asset records
* editable physical Project source
* read-only virtual Engine source for built-in resources
* separated Project Content and Engine Content source groups
* source-aware asset counts, filters, and diagnostics
* automatic asset import and registration
* import into the currently selected physical folder
* asset file dialog
* asset reload and reimport workflows
* searchable assets and folders by ID, name, path, or type
* All / Meshes / Textures / Audio / Scripts filters
* Issues / Missing / External diagnostics filters
* Browser and List content views
* Tiny / Small / Medium / Large / Extra Large thumbnail presets
* texture thumbnail previews and mesh glyph fallback
* persistent asset details panel
* responsive content and details split layout
* texture previews and source diagnostics
* physical project-folder Sources tree
* breadcrumb, parent-folder, Back, and Forward navigation
* non-recursive current-folder content views
* physical child-folder cards and rows
* create and rename physical folders
* recursive physical-folder deletion with validation and rollback
* folder navigation, context menus, and asset drop targets
* multi-selection with Ctrl, Shift, and Ctrl+Shift
* shared Browser/List selection state
* centralized asset operation command layer
* project-scoped Copy / Cut / Paste clipboard
* single and batch asset duplication
* physical asset-file rename with registry updates
* remove single or multiple assets from the registry
* transactional project-file deletion with rollback protection
* active-scene reference validation before destructive deletion
* selection, history, and clipboard repair after destructive operations
* item and empty-content context menus
* typed Mesh, Texture, Audio, and Script drag payloads
* drag/drop assignment into Inspector asset pickers
* drag/drop Static Mesh actor creation in the Viewport
* built-in primitive actor creation from the Engine source
* Undo/Redo integration for created primitive and dropped mesh actors
* operation confirmation, validation, and feedback modals
* Lua Script source validation with Valid / Error status
* direct Script source opening through double-click and context-menu actions
* Script asset references included in active-scene reference validation

### Audio

Quiet currently uses miniaudio as its native audio backend while keeping backend types private to the audio implementation.

Current foundation:

* miniaudio engine owned by the application-level AudioSubsystem
* Quiet-native AudioLibrary and AudioAsset metadata
* WAV, FLAC, and MP3 project assets
* non-spatial editor asset preview
* persistent Audio Source and Audio Listener ECS components
* backend-neutral Actor UUID runtime playback API
* Play, Stop, Pause, Resume, Seek, and playback-state queries
* Play On Start and looping
* independent source volume and pitch
* 2D / non-spatial playback
* primary runtime Audio Listener selection
* source and listener position/orientation synchronization from runtime Transforms
* inverse distance attenuation with Min Distance, Max Distance, and Rolloff
* directional source cones
* source and listener velocity synchronization
* Doppler
* runtime Audio Source creation, removal, destruction, and asset-change synchronization
* Master, Music, SFX, and UI audio buses
* persistent scene-wide Master / Music / SFX / UI mix levels
* runtime mix application with restoration of editor/application mixer state when runtime ends
* Edit / Simulate / Play audio isolation

Persistent ECS and scene data contain no miniaudio types. Runtime `ma_sound`, sound groups, playback cursors, listener state, and backend device state remain inside the Quiet audio runtime implementation.

Advanced environmental acoustics are intentionally outside this foundation and are planned for a later Steam Audio phase.

### Console / Diagnostics

* editor console panel
* filtered log display
* copy logs to clipboard
* diagnostics/debug panel
* renderer and viewport metrics

---

## Runtime Modes

Quiet currently supports three editor scene modes.

### Edit Mode

Normal editor mode.

* scene editing enabled
* actor selection enabled
* hierarchy editing enabled
* inspector editing enabled
* scene settings editing enabled
* gizmos enabled
* undo/redo enabled

### Simulate Mode

Runtime simulation mode.

* runtime scene clone
* editor scene remains isolated
* editing controls are disabled for runtime safety
* simulation can be stopped and returned to Edit Mode

### Play Mode

Runtime play-testing mode.

* runtime scene clone
* editor interaction disabled
* mouse capture enabled for runtime testing
* returns to Edit Mode when stopped

---

## Physics

Quiet uses Jolt Physics for runtime rigid-body simulation.

Current physics foundation:

* Jolt subsystem initialization and shutdown owned by the engine
* isolated PhysicsWorld owned by each active runtime Scene
* fixed 60 Hz simulation with bounded catch-up stepping
* Static, Dynamic, and Kinematic rigid-body motion types
* ECS-authored Rigid Body components with no persistent Jolt types
* Box Collider support
* Sphere Collider support
* Capsule Collider support
* one primitive collider per Actor
* friction and restitution
* linear and angular damping
* gravity factor
* sleeping control
* scene-wide configurable gravity
* runtime impulse application
* runtime linear-velocity control
* Dynamic body transform synchronization from physics to ECS
* Kinematic body synchronization from ECS to physics
* normalized Actor-pair contact Started / Ended events
* editor collider wireframe visualization

Physics data is authored as Quiet scene/component data. Jolt body identifiers and runtime objects remain internal to the physics runtime.

Current transform authority is:

```text
Static
    authored Transform → physics body

Dynamic
    physics body → Transform

Kinematic
    Transform → physics body
```

Physics runtime state belongs to the Simulate / Play scene copy and does not modify the Edit scene.

---

## Project Workflow

Quiet uses `.quiet` project files.

Normal project startup is handled by the standalone `Quiet_Launcher`.

The launcher can discover existing projects, track recent projects, create new projects, and launch the Editor with an explicit project descriptor.

Editor startup is staged across rendered frames:

```text
Prepare Editor
    ↓
Read Project
    ↓
Load Mesh Assets
    ↓
Load Texture Assets
    ↓
Load Startup Scene
    ↓
Editor Ready
```

A project descriptor stores:

* project name
* project version
* created/updated timestamps
* startup scene
* scenes directory
* assets directory
* cache directory

Typical project layout:

```text
MyProject/
├── MyProject.quiet
├── AssetRegistry.json
├── Scenes/
│   └── DefaultScene.json
├── Assets/
│   ├── Meshes/
│   └── Textures/
└── Cache/
```

The editor can:

* open an existing project
* create a project
* track recent projects
* open project scenes
* set a startup scene
* reload project assets
* navigate editable physical Project content and read-only Engine resources
* import and automatically register assets in the selected physical folder
* create, rename, and recursively delete physical asset folders
* move registered assets between folders with drag/drop
* multi-select, copy, cut, paste, duplicate, rename, remove, and delete project assets
* protect destructive operations with validation, confirmation, staging, and rollback
* diagnose missing, invalid, pathless, and external source paths
* create built-in primitive actors from the Engine source

---

## Scene Format

Scenes are saved as JSON.

Current serialized data includes:

* actors
* transforms
* static mesh components
* material data
* directional light components
* rotator components
* camera components
* rigid body components
* Box Collider components
* Sphere Collider components
* Capsule Collider components
* Audio Source components
* Audio Listener components
* scene render settings
* scene physics settings
* scene audio mix settings
* editor hierarchy metadata

Example hierarchy section:

```json
"editor_hierarchy": {
    "root": [
        {
            "type": "actor",
            "id": "..."
        },
        {
            "type": "folder",
            "id": "..."
        }
    ],
    "folders": [
        {
            "id": "...",
            "name": "Folder",
            "expanded": true,
            "children": []
        }
    ]
}
```

Hierarchy metadata is editor-only and does not affect runtime transforms.

---

## Resources

Engine resources live under:

```text
Engine/Resources/
├── Fonts/
├── Shaders/
└── Textures/
```

Build behavior:

* fonts are copied to the target output directory
* textures are copied to the target output directory
* shaders are compiled with `glslc`
* compiled shaders are written to the target output `Resources/Shaders` directory

---

## Branding Assets

Current branding files:

```text
Docs/Brand/quiet-logo.png
Editor/Resources/Windows/QuietEditor.ico
Editor/Resources/Windows/QuietEditor.rc
Engine/Resources/Textures/Editor/quiet-icon.png
Engine/Resources/Textures/Editor/quiet-topbar.png
```

Usage:

* `quiet-logo.png` is used by this README
* `QuietEditor.ico` is embedded into the Windows editor executable
* `quiet-icon.png` is used as the GLFW window icon
* `quiet-topbar.png` is loaded as an editor texture and shown in the topbar

---

## Fonts and Icons

Included editor UI assets:

* Inter
* JetBrains Mono
* Font Awesome Free Solid

These are used for editor text, mono text, toolbar icons, viewport icons, and compact UI controls.

---

## Build Requirements

Required locally:

* C++20 compiler
* CMake 3.28+
* Ninja
* Vulkan SDK
* `glslc` from the Vulkan SDK

Fetched automatically by CMake:

* GLFW
* spdlog
* GLM
* Dear ImGui docking branch
* ImGuizmo
* ImGuiFileDialog
* nlohmann/json
* Assimp
* stb
* EnTT
* Jolt Physics
* miniaudio

---

## Build

### Sandbox (Debug)
```bash
cmake --preset debug
cmake --build --preset build-debug
```

### Sandbox (Release)
```bash
cmake --preset release
cmake --build --preset build-release
```

### Editor (Debug)
```bash
cmake --preset debug-editor
cmake --build --preset build-debug-editor
```

### Editor (Release)
```bash
cmake --preset release-editor
cmake --build --preset build-release-editor
```

### Launcher (Debug)
```bash
cmake --preset debug-launcher
cmake --build --preset build-debug-launcher
```

### Launcher (Release)
```bash
cmake --preset release-launcher
cmake --build --preset build-release-launcher
```


You can also build the editor target directly:

```bash
cmake --build Build/Intermediate/Debug-Editor --target Quiet_Editor -j 14
```

---

## Output Layout

Executables and resources are written to:

```text
Build/Binaries/<Config>/
```

Typical debug launcher/editor output:

```text
Build/Binaries/Debug/
├── Quiet_Launcher
├── Quiet_Editor
└── Resources/
    ├── Fonts/
    ├── Shaders/
    └── Textures/
```

On Windows with MinGW, required runtime DLLs are copied next to the executable.

---

## Useful Editor Shortcuts

```text
Ctrl+N              New Scene
Ctrl+S              Save Scene
Ctrl+O              Load Scene
Ctrl+Z              Undo
Ctrl+Y              Redo
Ctrl+Shift+Z        Redo
Ctrl+D              Duplicate selected actor or focused Asset selection
Delete              Delete selected actor or focused Asset selection
Ctrl+C              Copy focused Project Asset selection
Ctrl+X              Cut focused Project Asset selection
Ctrl+V              Paste Project Assets into the current folder
F2                  Rename the selected Project Asset
F                   Focus selected actor
F5                  Play / Stop Play
F6                  Simulate / Stop Simulate
T                   Translate gizmo
R                   Rotate gizmo
S                   Scale gizmo
L                   Toggle Local / World gizmo mode
G                   Toggle snapping
RMB                 Fly through the Viewport
Alt+LMB             Orbit around the resolved pivot
MMB                 Pan parallel to the camera plane
Mouse Wheel         Adjust camera speed during Fly
Shift               Apply 2x Fly speed boost
```

---

## Current Development Roadmap

### Completed Recently

* dark professional editor style pass
* high-DPI editor scaling pass
* default dock layout refinement
* editor branding pass
* persistent hierarchy metadata
* hierarchy folders
* hierarchy drag/drop organization
* hierarchy metadata validation
* Scene Settings panel
* scene render settings dirty-state and undo/redo support
* Asset Browser search, type filters, and diagnostics filters
* persistent and responsive asset details region
* Browser/List content views with five thumbnail presets
* physical Project source and read-only virtual Engine source
* separated Project Content and Engine Content source groups
* physical folder tree, breadcrumbs, Back/Forward history, and child-folder content
* multi-selection shared between Browser and List views
* centralized asset operations and project-scoped clipboard
* single and batch Copy, Cut, Paste, Duplicate, Rename, Remove, and Delete workflows
* transactional asset and recursive folder deletion with validation and rollback
* stable item and background context-menu targeting
* searchable Inspector Mesh and Texture asset pickers
* drag/drop Mesh and Texture assignment in the Inspector
* drag/drop Static Mesh actor creation in the Viewport
* built-in primitive creation from the Engine source
* imported material-slot Inspector presentation and compact texture previews
* procedural adaptive world grid with major lines and world axes
* camera-height grid LOD and horizon-aware fading
* configurable Viewport camera speed and presets
* formal Viewport cursor capture and restoration
* RMB Fly, Alt+LMB Orbit, and MMB Pan navigation
* shared free-camera and piloted-camera navigation
* contextual Viewport navigation guide and live status feedback
* mesh and editor-icon hover feedback
* depth-aware GPU silhouette highlighting for selected Static Mesh actors
* live mesh-drop placement preview with ground alignment
* consolidated Viewport display options
* Inspector component reset workflow for Transform, Material, Directional Light, Rotator, and Camera
* typed Inspector component clipboard for Transform, Static Mesh, Material, Directional Light, Rotator, and Camera
* validated component paste availability with contextual disabled-action feedback
* complete Inspector component Undo/Redo and dirty-state integration
* editable gizmo snapping increments with compact Viewport configuration
* editor-only selection-mask render target and selected-mesh mask pass
* fullscreen selection-outline composite with resize-safe Vulkan resource handling
* standalone Quiet Launcher application foundation
* launcher project discovery, creation, and Editor process handoff
* isolated Launcher and Editor ImGui layout state
* visible staged Editor project bootstrap
* incremental startup mesh and texture loading
* EnTT dependency and stable Actor component API
* registry-backed Actor identity and component storage
* lightweight Actor handles with explicit Scene cloning for Edit / Simulate / Play isolation
* storage-independent scene history, serialization, Editor, Viewport, and Inspector workflows
* EnTT runtime views for component-system iteration
* EnTT-driven Static Mesh and selection-mask rendering paths
* Jolt Physics subsystem and per-runtime-scene PhysicsWorld
* fixed-step rigid-body simulation
* Static, Dynamic, and Kinematic transform-authority model
* persistent Rigid Body authoring and serialization
* Box, Sphere, and Capsule collider authoring and runtime shapes
* runtime impulse and linear-velocity controls
* Actor-pair contact Started / Ended event foundation
* editor collider debug visualization
* serialized scene gravity with Scene Settings controls
* miniaudio AudioSubsystem foundation with backend-private engine ownership
* Audio project assets with WAV / FLAC / MP3 metadata loading and editor preview
* persistent Audio Source and Audio Listener ECS components
* backend-neutral runtime Audio Source playback controls
* primary runtime Audio Listener selection
* 2D and 3D runtime source playback
* runtime listener and source transform synchronization
* distance attenuation and directional source cones
* source/listener velocity synchronization and Doppler
* runtime Audio Source lifecycle synchronization
* Master / Music / SFX / UI runtime bus routing
* persistent scene-wide audio mix levels with runtime mixer isolation

### Upcoming Areas

* asset cooking and runtime pipeline
* text and runtime UI
* navigation with Recast + Detour
* advanced gameplay and physics systems
* advanced acoustics with Steam Audio later
* runtime rendering expansion
* build and distribution

---

## Long-Term Direction

Quiet is being developed as a Vulkan-based engine and editor with emphasis on:

* explicit engine/runtime/editor separation
* practical tooling
* predictable scene workflows
* modern C++ architecture
* scalable rendering systems
* editor-first iteration speed

The goal is not to rush features, but to build a stable, understandable engine foundation one system at a time.
