<p align="center">
  <img src="Docs/Brand/quiet-logo.png" alt="Quiet Engine" width="512">
</p>

<h1 align="center">QUIET</h1>

Quiet is a C++20 Vulkan engine project with an integrated editor workflow.

The current focus is an editor-first engine foundation: scene editing, project management, asset registration, viewport tooling, runtime mode switching, serialization, undo/redo, and a modern Dear ImGui-based editor UI.

---

## Current Status

Quiet is in active prototype development.

Implemented foundations:

* Vulkan renderer with swapchain, depth resources, frame synchronization, scene viewport render target, and static mesh rendering
* Dear ImGui dockspace editor shell
* dark professional editor theme
* high-DPI aware editor UI scaling
* scene viewport with picking, overlays, camera controls, and ImGuizmo editing
* project manager with `.quiet` project descriptors
* scene save/load workflow
* project-local asset registry
* mesh and texture import/loading foundation
* editor hierarchy with folders, drag/drop ordering, and persistent hierarchy metadata
* inspector with component editing
* undo/redo and dirty-state tracking
* Edit / Simulate / Play runtime mode groundwork
* console and diagnostics panels

Quiet is not a production engine yet. It is being built step by step with a strong focus on clean architecture and practical editor tooling.

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
* top toolbar
* resettable default layout
* project manager modal
* docked panels:

  * Viewport
  * Scene / Hierarchy
  * Inspector
  * Assets
  * Console
  * Debug

### Viewport

* scene viewport render target
* viewport picking
* selected actor interaction
* camera navigation
* viewport overlay toolbar
* status overlays
* ImGuizmo integration
* translate / rotate / scale tools
* local/world gizmo mode
* snapping support
* focus selected actor

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

* actor header
* transform editing
* static mesh editing
* material editing
* directional light component editing
* rotator component editing
* camera component editing
* add component menu
* component action menu
* reset transform action

### Assets

* project-local asset registry
* mesh and texture asset records
* built-in resources
* asset reload action
* asset registration UI
* asset file dialog
* compact asset table
* texture preview support

### Console / Diagnostics

* editor console panel
* filtered log display
* copy logs to clipboard
* diagnostics/debug panel
* renderer and viewport metrics

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
├── Scenes/
│   └── DefaultScene.json
├── Assets/
└── Cache/
```

The editor can:

* open an existing project
* create a project
* track recent projects
* open project scenes
* set a startup scene
* reload project assets

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
Ctrl+D              Duplicate Selected
Delete              Delete Selected
F                   Focus Selected
F5                  Play / Stop Play
F6                  Simulate / Stop Simulate
```

---

## Current Development Roadmap

### Recently Completed

* dark professional editor style pass
* high-DPI editor scaling pass
* default dock layout refinement
* persistent hierarchy metadata
* hierarchy folders
* hierarchy drag/drop organization
* hierarchy metadata validation

### Immediate Next Work

* Scene / World Settings panel
* move scene lighting and ambient controls out of actor Inspector
* scene settings dirty-state and undo/redo support
* scene settings visual polish

### Upcoming Areas

* asset browser upgrade
* asset context menus
* reimport / unregister asset workflows
* better viewport grid and selection visuals
* snapping controls
* improved project workflow
* runtime/play-mode expansion
* component copy/paste and inspector workflow polish

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
