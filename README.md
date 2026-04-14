# Quiet

Quiet is a C++20 Vulkan engine project focused on building a clean real-time runtime with an integrated in-engine editor workflow.

The project is organized as a multi-target CMake workspace and currently includes:

* a core engine library,
* a sandbox runtime target,
* an editor UI layer built with Dear ImGui.

---

## Current Features

### Core Engine

* GLFW windowing and input system
* Vulkan instance / surface / device / swapchain setup
* dynamic rendering pipeline
* static mesh rendering
* runtime mesh library (built-in primitives)
* free camera controls

### Scene System

* actor/component-based scene representation
* UUID-based actor identity
* scene serialization (save/load)
* actor creation, duplication, deletion
* dirty scene tracking with save indicator

### Editor

* ImGui-based editor shell:

  * dockspace layout
  * top toolbar (icon-based)
  * scene viewport
  * hierarchy
  * inspector
  * viewport overlay toolbar
  * console (spdlog-backed)
* actor selection (hierarchy + viewport picking)
* ImGuizmo transform editing (translate / rotate / scale)
* gizmo modes (local/world)
* snapping system
* editor input routed through engine input system

### Runtime Modes

* **Edit Mode**

  * full scene editing
  * gizmos and selection enabled

* **Simulate Mode**

  * runtime scene clone
  * simulation without affecting editor scene

* **Play Mode**

  * runtime scene clone
  * editor interactions disabled
  * mouse capture enforced
  * intended for gameplay testing

### Resources

* engine resources system:

  * `resources/fonts`
  * `resources/shaders`
* runtime deployment to:

  * `Build/Binaries/<Config>/resources`
* shader compilation pipeline using `glslc`

### Fonts & Icons

* UI fonts: Inter (Regular / Medium / Bold)
* Mono font: JetBrains Mono
* Icon font: Font Awesome (Free Solid)

Icons are used for:

* top toolbar actions (new, save, load, play, simulate)
* viewport toolbar (gizmo tools, snapping, focus)

---

## Project Structure

```text
Engine/
    include/
    src/
    resources/
        fonts/
        shaders/

Sandbox/
    src/

Editor/
    src/ (future dedicated editor entry point)

Build/
    Binaries/
    Intermediate/
```

---

## Build Requirements

* C++20 compiler
* CMake (3.28+ recommended)
* Vulkan SDK
* GLFW
* Dear ImGui (docking branch)
* GLM
* spdlog
* nlohmann/json

---

## Build

```bash
cmake -S . -B Build
cmake --build Build
```

Outputs:

```text
Build/Binaries/<Config>/
    Quiet_Sandbox.exe
    resources/
    Shaders/
```

---

## Controls

### General

* `Shift + F1` → toggle mouse capture

### Editor

* `Ctrl + S` → save scene
* `F` → focus selected actor

### Gizmos

* `T` → translate
* `R` → rotate
* `S` → scale
* `L` → toggle local/world
* `G` → toggle snapping

### Runtime

* `F5` → Play / Stop Play
* `F6` → Simulate / Stop Simulate

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

---

## Font & Icon Sources

* **Inter Font**
  https://rsms.me/inter/

* **JetBrains Mono**
  https://www.jetbrains.com/lp/mono/

* **Font Awesome Free (Solid)**
  https://fontawesome.com/

All fonts are used for development purposes and are included in `Engine/resources/fonts`.
