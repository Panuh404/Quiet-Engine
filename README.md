# Quiet

Quiet is a modern **C++20** game engine project built with **CMake**, designed as a clean multi-target workspace for engine development.

The long-term goal of the project is to build a **Vulkan-based 3D game engine**
## Project Structure

- **Engine** — core engine library where runtime systems, platform code, rendering, and shared engine functionality live.
- **Sandbox** — lightweight test application used to validate engine features during development.
- **Editor** — early editor target reserved for future tooling and content workflows.

## Current Status

The project is currently in its **foundation stage**:

- CMake workspace and presets are configured
- Engine, Sandbox, and Editor targets are separated
- Public engine headers are isolated from private implementation
- Final build artifacts are written to `Build/Binaries`
- Intermediate/generated files are written to `Build/Intermediate`

At this stage, the codebase is focused on establishing a clean architecture and reliable build pipeline before introducing platform and rendering systems.

## Development Direction

Planned next milestones include:

- desktop window creation
- Vulkan SDK integration
- renderer initialization and frame loop
- resource management
- scene rendering
- gameplay support for a tiny 3D shooter

# Notes

This repository is being developed with an emphasis on:

- clean engine/game separation
- maintainable architecture
- debuggable systems
- incremental, testable milestones
- modern C++ and graphics programming practices