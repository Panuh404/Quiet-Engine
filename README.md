# Quiet

Quiet is a C++20 Vulkan engine project focused on building a clean real-time runtime and an in-engine editor workflow.

The project is organized as a multi-target CMake workspace and currently includes:
- a core engine library,
- a sandbox runtime target,
- an editor-oriented UI layer built with Dear ImGui.

## Current Features

Quiet currently includes:

- GLFW windowing and input
- Vulkan instance / surface / device / swapchain setup
- dynamic rendering pipeline
- static mesh rendering
- free camera controls
- scene representation with actors and components
- ImGui-based editor shell with:
    - dockspace
    - topbar placeholder
    - scene viewport
    - hierarchy
    - inspector
    - viewport overlay toolbar
- actor selection from hierarchy and viewport
- ImGuizmo transform editing
- runtime mesh library with built-in primitive meshes

## Project Structure

- `Engine/` — core engine code, renderer, platform, scene, editor UI support
- `Sandbox/` — runtime test application used to validate engine features
- `Editor/` — reserved for future dedicated editor target / startup path

## Build Requirements

- C++20 compiler
- CMake
- Vulkan SDK
- GLFW
- Dear ImGui
- GLM
- spdlog

## Current Focus

The current development focus is shifting from raw rendering bring-up into editor and scene workflow features.

Immediate priorities are:

- refactoring `Application` into cleaner renderer/editor orchestration
- scene serialization
- UUID-based actor identity
- topbar actions for save/load/new scene
- continued editor workflow polish

## Long-Term Direction

Quiet is being developed as a Vulkan-based engine with an integrated tooling workflow, with an emphasis on:

- clean architecture
- explicit engine/editor separation
- maintainable rendering code
- practical editor features built incrementally
- modern C++ and graphics programming practices