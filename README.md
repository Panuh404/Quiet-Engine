# Quiet

Quiet is a C++20 Vulkan engine project focused on building a clean real-time runtime with an integrated editor workflow.

The workspace currently contains:

- `Quiet_Engine` — core static engine library
- `Quiet_Sandbox` — runtime executable
- `Quiet_Editor` — optional editor executable

Quiet is currently in an editor-first prototype stage: the renderer foundation, scene editing workflow, serialization, viewport tooling, and runtime mode switching are already in place.

---

### Editor

- Dear ImGui dockspace-based editor shell
- top toolbar
- scene viewport
- hierarchy panel
- inspector panel
- console panel
- viewport overlay toolbar
- viewport picking
- ImGuizmo transform editing
- translate / rotate / scale tools
- local/world gizmo mode
- snapping support
- editor command/action layer

### Runtime Modes

- **Edit Mode**
  - full scene editing
  - actor selection and gizmos enabled

- **Simulate Mode**
  - runtime scene clone
  - simulation isolated from the editor scene

- **Play Mode**
  - runtime scene clone
  - editor interaction disabled
  - mouse capture enforced for runtime testing

### Resources

- engine resources live under:
  - `Engine/Resources/Fonts`
  - `Engine/Resources/Shaders`
  - `Engine/Resources/Textures`
- fonts and textures are copied per target after build
- shaders are compiled with `glslc` into the target output resources directory

### Fonts and Icons

Included UI assets:

- [Inter](https://rsms.me/inter/)
- [JetBrains Mono](https://www.jetbrains.com/lp/mono/)
- [Font Awesome Free Solid](https://fontawesome.com/)

These are used by the editor for text styling, mono text, and toolbar/viewport icons.

---

## Project Structure

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
│
├── Engine/
│   ├── CMakeLists.txt
│   ├── include/Quiet/
│   │   ├── Core/
│   │   ├── Editor/
│   │   ├── Input/
│   │   ├── Math/
│   │   ├── Platform/
│   │   ├── Renderer/
│   │   └── Scene/
│   ├── src/
│   │   ├── Core/
│   │   ├── Editor/
│   │   ├── Input/
│   │   ├── Platform/
│   │   ├── Renderer/Vulkan/
│   │   └── Scene/
│   └── Resources/
│       ├── Fonts/
│       ├── Shaders/
│       └── Textures/
│
├── Sandbox/
│   ├── CMakeLists.txt
│   └── src/
│
├── Editor/
│   ├── CMakeLists.txt
│   └── src/
│
└── Build/
    ├── Binaries/
    └── Intermediate/
```
---

## Build Requirements
### Required locally
* C++20 compiler
* CMake (3.28+ recommended)
* Vulkan SDK
### Fetched automatically
* [GLFW](https://github.com/glfw/glfw)
* [spdlog](https://github.com/gabime/spdlog)
* [GLM](https://github.com/g-truc/glm)
* [Dear ImGui](https://github.com/ocornut/imgui/tree/docking)
* [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
* [nlohmann/json](https://github.com/nlohmann/json)

---

## Build
Debug:
```bash
cmake --preset debug
cmake --build --preset build-debug
```

Release:
cmake --preset release
cmake --build --preset build-release

### Output Layout
Executable are written to: `Build/Binaries/<Config>/`

Typical outputs include:
```text
Build/Binaries/Debug/
├── Quiet_Sandbox           # when QUIET_BUILD_SANDBOX=ON
├── Quiet_Editor            # when QUIET_BUILD_EDITOR=ON
└── Resources/
    ├── Fonts/
    ├── Shaders/
    └── Textures/
```

---

## Current Focus

Development is currently focused on:

* editor workflow polish
* scene/runtime separation
* input system consistency
* resource management
* preparing the renderer for more advanced features

---

## Long-Term Direction

Quiet is being developed as a Vulkan-based engine with an integrated editor, with emphasis on:

* clean architecture
* explicit engine/runtime/editor separation
* scalable rendering systems
* practical tooling
* modern C++ practices
