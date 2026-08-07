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
* Dear ImGui dockspace editor shell
* dark professional editor theme
* high-DPI aware editor UI scaling
* scene viewport with an adaptive world grid, actor picking, hover/selection feedback, Fly/Orbit/Pan navigation, mesh-placement previews, and ImGuizmo editing
* project manager with `.quiet` project descriptors
* scene save/load workflow
* dual-source Asset Browser with editable physical Project content, read-only Engine resources, and a project-local asset registry
* mesh and texture import/loading foundation
* editor hierarchy with folders, drag/drop ordering, and persistent hierarchy metadata
* dedicated Scene Settings panel for scene-wide lighting and ambient controls
* inspector with validated component editing, reset, copy/paste, undo/redo, and dirty-state integration
* undo/redo and dirty-state tracking
* Edit / Simulate / Play runtime mode groundwork
* console and diagnostics panels
* editor branding with application icon, window icon, README logo, and topbar logo

---

## Workspace Targets

The workspace currently contains three main targets:

```text
Quiet_Engine   - core static engine library
Quiet_Sandbox  - runtime sandbox executable
Quiet_Editor   - editor executable
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
* distinct hover and selected-actor feedback
* projected mesh bounds, actor names, and origin markers
* Camera and Directional Light editor icons and overlays
* configurable Viewport display options
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
* snapping support
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
* Add Component menu
* consistent component action menus
* Reset Component support for Transform, Material, Directional Light, Rotator, and Camera
* typed component-value clipboard for Transform, Static Mesh, Material, Directional Light, Rotator, and Camera
* cross-actor Copy Component and Paste Component Values workflows
* asset, slot-count, and component-type validation for pasted values
* contextual explanations for unavailable Paste actions
* atomic undo/redo and dirty-state integration for component actions and value editing
* bounded numeric validation for Transform scale, lighting, material colors, and Camera projection values
* searchable Mesh and Texture asset pickers
* asset search by registry ID and path
* texture previews and source diagnostics in picker results
* drag/drop Mesh and Texture assignment

### Scene Settings

Scene-wide render settings live in a dedicated panel instead of the actor Inspector.

Current controls:

* directional scene light direction
* directional scene light intensity
* ambient color
* ambient intensity
* normalize light direction
* reset directional lighting
* reset ambient
* reset all scene lighting settings

Scene Settings are serialized with the scene and are tracked by dirty-state and undo/redo.

### Assets

* project-local asset registry
* Mesh and Texture asset records
* editable physical Project source
* read-only virtual Engine source for built-in resources
* separated Project Content and Engine Content source groups
* source-aware asset counts, filters, and diagnostics
* automatic asset import and registration
* import into the currently selected physical folder
* asset file dialog
* asset reload and reimport workflows
* searchable assets and folders by ID, name, path, or type
* All / Meshes / Textures filters
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
* typed Mesh and Texture drag payloads
* drag/drop assignment into Inspector asset pickers
* drag/drop Static Mesh actor creation in the Viewport
* built-in primitive actor creation from the Engine source
* Undo/Redo integration for created primitive and dropped mesh actors
* operation confirmation, validation, and feedback modals

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

## Project Workflow

Quiet uses `.quiet` project files.

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
* scene render settings
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

---

## Build

### Debug Sandbox

```bash
cmake --preset debug
cmake --build --preset build-debug
```

### Release Sandbox

```bash
cmake --preset release
cmake --build --preset build-release
```

### Debug Editor

```bash
cmake --preset debug-editor
cmake --build --preset build-debug-editor
```

### Release Editor

```bash
cmake --preset release-editor
cmake --build --preset build-release-editor
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

Typical debug editor output:

```text
Build/Binaries/Debug/
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
* selected-actor projected bounds and origin feedback
* live mesh-drop placement preview with ground alignment
* consolidated Viewport display options
* Inspector component reset workflow for Transform, Material, Directional Light, Rotator, and Camera
* typed Inspector component clipboard for Transform, Static Mesh, Material, Directional Light, Rotator, and Camera
* validated component paste availability with contextual disabled-action feedback
* complete Inspector component Undo/Redo and dirty-state integration

### Upcoming Areas

* editable snapping increment controls
* GPU silhouette/post-process selection highlighting
* recent scenes per project
* better unsaved-change prompts
* project settings
* runtime/play-mode expansion
* component copy/paste

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
