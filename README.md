# QUIET ENGINE

 Quiet is an open source Game Engine in Development made with education and simplicity in mind. A Game Engine is a set of software tools that optimizes the development of video games.

### *QUIET ONLY SUPPORTS WINDOWS CURRENTLY*

## Roadmap
* Renderer
* Render API Abstraction (OpenGL, Vulkan, Direct3D)
* Debugging Support (Logging, Profiling, etc)
* Scripting Language (C# / Python) <Not Decided Yet>
* Memory Systems
* Physics
* File I/O, VFS

## Progress
* Project Initial Setup:
    - Engine Project -> Quiet   -> Static Library `.lib`
    - Test Project   -> Sandbox -> Executable `.exe`

* Entry Point:
    - Basic Application
    - Entry Point

* Logging:
    - Quiet Core Logger   -> Quiet Logging Tool
    - Quiet Client Logger -> App Logging Tool
    - Basic Logging API Macros

* Premake:
    - Generate project Files (`./scripts/GenerateProject_Win.bat`)
    - Main Project Premake config (`./premake5.lua`)
    - Quiet Dependencies Premake config (`./Quiet/vendor/premake5.lua`)

* Event System:
    - Event Class
    - Application Events
    - Key Events
    - Mouse Events
    - Event Dispatcher
    
* Window Abstraction:
    - Window Class -> Windows Window (Platform `Windows`)
    - Window Events -> GLFW Callbacks

* Layers:
    - Layer Stack
    - Applicable to Events and logic
    - Determine the order of Drawing

* User Interface:
    - ImGui Layer
    - ImGui OpenGL Renderer
    - ImGui Events

* Input:
    - Input Class -> Windows Input -> GLFW Input System
    - Input Polling
    - Key and Mouse Codes

* ImGui:
    - ImGuiBuild -> Implementation of Dear ImGui Backend (OpenGL / glfw)
    - Dockspace
    - Viewports

* Renderer:
    - OpenGL Rendering
    - Shaders GLSL
    - Cameras
    - Textures

## Dependencies
* [spdlog](https://github.com/gabime/spdlog) -> Fast C++ logging library 
* [Premake](https://github.com/premake/premake-core) -> Generate Project Files 
* [GLFW](https://github.com/glfw/glfw) -> A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input 
* [Glad](https://github.com/Dav1dde/glad) -> GL/GLES/EGL/GLX/WGL Loader-Generator 
* [Dear ImGui](https://github.com/ocornut/imgui) -> Bloat-free Graphical User interface for C++ 
* [glm](https://github.com/g-truc/glm) -> OpenGL Mathematics 
* [stb](https://github.com/nothings/stb) -> single-file public domain libraries (stb_image.h only)

## Resources
OpenGL
* http://docs.gl/
* https://www.khronos.org/opengl/wiki/
* https://learnopengl.com/

DirectX
* http://www.rastertek.com/
* https://www.3dgep.com/

Vulkan
* https://vulkan-tutorial.com/
* https://youtube.com/playlist?list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR
* https://github.com/SaschaWillems/Vulkan

Graphics General
* https://www.scratchapixel.com/

Math
* https://www.youtube.com/playlist?list=PL7wAPgl1JVvUEb0dIygHzO4698tmcwLk9
* http://www.euclideanspace.com/
* https://eater.net/quaternions

## Main features to come:
- Fast 2D rendering (UI, particles, sprites, etc.)
- High-fidelity Physically-Based 3D rendering (this will be expanded later, 2D to come first)
- Ray Tracing
- Support for Mac, Linux
- Native rendering API support (DirectX, Vulkan)
- Fully featured viewer and editor applications
- Fully scripted interaction and behavior
- Integrated 3rd party 2D and 3D physics engine
- Procedural terrain and world generation
- Artificial Intelligence
- Audio system

## License
Quiet engine is licensed under MIT License. See [LICENSE](LICENSE) file.