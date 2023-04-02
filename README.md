# QUIET ENGINE

 Quiet is an open source Game Engine in Development made with education and simplicity in mind. A Game Engine is a set of software tools that optimizes the development of video games.

### *QUIET ONLY SUPPORTS WINDOWS CURRENTLY*

## Project Setup
(This project is compatible with Visual Studio 2017 and 2022, other IDE has not been tested yet.)

Start cloning the repository with `git clone --recursive https://github.com/Panuh404/Quiet-Engine.git`.
If the repository was cloned non-recursively previously, use `git submodule update --init` to clone the necessary submodules.
You need to run the script to generate the visual studio (/scripts/GenProject_VS2022.bat)

## Roadmap
* Renderer 2D
* Renderer 3D
* Render API Abstraction (OpenGL, Vulkan, Direct3D)
* Debugging Support (Logging, Profiling, etc)
* Scripting Language
* Memory Systems
* Physics
* File I/O, VFS

## Vendor information
* [spdlog](https://github.com/gabime/spdlog) -> Fast C++ logging library 
* [Premake](https://github.com/premake/premake-core) -> Generate Project Files 
* [GLFW](https://github.com/glfw/glfw) -> A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input 
* [Glad](https://github.com/Dav1dde/glad) -> GL/GLES/EGL/GLX/WGL Loader-Generator 
* [Dear ImGui](https://github.com/ocornut/imgui) -> Bloat-free Graphical User interface for C++ 
* [glm](https://github.com/g-truc/glm) -> OpenGL Mathematics 
* [stb](https://github.com/nothings/stb) -> single-file public domain libraries (stb_image.h only)

## Resources
GameEngine Stuff
* https://thatonegamedev.com/cpp/c-packages-for-a-custom-game-engine/
* https://www.gamedeveloper.com/blogs/how-to-make-your-own-c-game-engine

Shaders
* https://thebookofshaders.com/
* https://www.pbr-book.org/

OpenGL
* http://docs.gl/
* https://www.khronos.org/opengl/wiki/
* https://learnopengl.com/
* https://ogldev.org/
* https://nehe.gamedev.net/
* https://paroj.github.io/gltut/index.html

DirectX
* http://www.rastertek.com/
* https://www.3dgep.com/
* http://www.directxtutorial.com/
* https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide?redirectedfrom=MSDN
* https://digitalerr0r.net/2015/08/19/quickstart-directx-12-programming/

Vulkan
* https://vulkan-tutorial.com/
* https://youtube.com/playlist?list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR
* https://github.com/SaschaWillems/Vulkan
* https://github.com/vinjn/awesome-vulkan

Graphics General
* https://www.scratchapixel.com/

Math
* https://www.youtube.com/playlist?list=PL7wAPgl1JVvUEb0dIygHzO4698tmcwLk9
* http://www.euclideanspace.com/
* https://eater.net/quaternions

Programming
* https://refactoring.guru/design-patterns/abstract-factory
* https://en.cppreference.com/w/cpp/language

## Main features to come:
- Fast 2D rendering (UI, particles, sprites, etc.)
- High-fidelity Physically-Based 3D rendering (2D to come first)
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