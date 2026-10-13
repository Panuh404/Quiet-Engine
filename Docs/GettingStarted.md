# Getting Started <!--! {#quiet_getting_started} -->

This guide introduces the tools and build targets needed to run Quiet.

The commands in this guide use PowerShell and run from the repository
root unless stated otherwise.

This guide covers prerequisites, target selection, configuration,
building, runtime resources, first launch, and troubleshooting.

See [Architecture](Architecture.md) for system boundaries and
[Building the documentation](Contributing/BuildingDocumentation.md)
for the separate optional documentation build.

## Prerequisites

| Requirement | Purpose |
| --- | --- |
| Git | Repository checkout, dependency fetching, and submodules |
| CMake 3.28 or newer | Root project configuration |
| C and C++20-capable toolchain | Engine and dependency compilation |
| Ninja | Generator used by the supplied presets |
| Vulkan development environment | Vulkan headers, loader library, and glslc shader compiler |
| Vulkan-capable GPU and driver | Editor and Sandbox rendering |
| Network access during initial configuration | FetchContent dependency downloads |
| Khronos validation layer | Required by Debug Editor and Sandbox Vulkan initialization |

Run these checks in the same terminal environment that will configure
the project:

```powershell
git --version
cmake --version
ninja --version
glslc --version
```

Expected result: each command prints version information without a
command-not-found error.

These checks establish tool availability, not complete compiler or
GPU compatibility.

The supplied presets select Ninja but do not specify a compiler.
The compiler must be available through the selected terminal environment
or IDE toolchain configuration.

An IDE-bundled tool may work inside the IDE without being available in
a separate PowerShell session.

## Dependency setup

The root CMake project downloads its declared dependencies through
FetchContent. Initial configuration can therefore take longer than
subsequent configuration.

Lua is compiled into an embeddable library as part of the build.
A separately installed Lua interpreter is not required by that target.

Doxygen and the theme under Docs/ThirdParty are used by the optional
documentation project, rather than normal engine builds.

## Choose a target

| Goal | Executable target | Debug configure preset | Debug build preset |
| --- | --- | --- | --- |
| Create or open projects | Quiet_Launcher | debug-launcher | build-debug-launcher |
| Run the Editor | Quiet_Editor | debug-editor | build-debug-editor |
| Run standalone runtime content | Quiet_Sandbox | debug-sandbox | build-debug-sandbox |
| Cook a project package | Quiet_Cooker | debug-cooker | build-debug-cooker |

Each target also has matching release presets, using release in place
of debug.

The Launcher configuration enables the Editor. Building Quiet_Launcher
also builds Quiet_Editor through an explicit target dependency.

Cooker configuration enables its additional conversion dependencies.
Choose the Launcher or Editor first when the goal is editing a project.

## Build directory layout

Configure presets keep their intermediate files separate:

| Target selection | Debug directory |
| --- | --- |
| Launcher | Build/Intermediate/Debug-Launcher |
| Editor | Build/Intermediate/Debug-Editor |
| Sandbox | Build/Intermediate/Debug-Sandbox |
| Cooker | Build/Intermediate/Debug-Cooker |

Release presets use the corresponding Release directory names.

Executables use shared configuration-specific output directories:

- Build/Binaries/Debug
- Build/Binaries/Release

Separate configure directories do not imply separate executable output
directories for every toolchain.

## Configure and build the Launcher

Open PowerShell in the repository root, using the compiler environment
selected for your build.

List the available presets:

```powershell
cmake --list-presets
cmake --build --list-presets
```

Configure the Debug Launcher:

```powershell
cmake --preset debug-launcher
```

Expected result: configuration and generation complete, with build
files under Build/Intermediate/Debug-Launcher.

Initial configuration downloads dependencies. A download or toolchain
failure must be resolved before continuing to the build.

Build using the matching build preset:

```powershell
cmake --build --preset build-debug-launcher
```

Expected result: the build succeeds and produces these Windows
executables:

- Build/Binaries/Debug/Quiet_Launcher.exe
- Build/Binaries/Debug/Quiet_Editor.exe

The Launcher target depends on the Editor target.
There is no need to build the Editor separately for this path.

## Runtime resources

The executable output directory contains a Resources directory.

CMake's post-build commands copy engine fonts and textures.
The shader target compiles engine shader sources to SPIR-V files under
Resources/Shaders for Editor and Sandbox use.

Keep Resources with the executables rather than copying an executable
alone to another directory.

The source fonts and textures live under Engine/Resources.
Compiled shaders are generated build outputs.

For the Launcher workflow, verify:

```powershell
Test-Path .\Build\Binaries\Debug\Quiet_Launcher.exe
Test-Path .\Build\Binaries\Debug\Quiet_Editor.exe
Test-Path .\Build\Binaries\Debug\Resources\Fonts
Test-Path .\Build\Binaries\Debug\Resources\Textures
Test-Path .\Build\Binaries\Debug\Resources\Shaders
```

Expected result: all five checks return True.
These checks verify presence rather than complete resource contents.

## First launch

Launch from the executable directory so relative resource paths resolve
against the generated Resources directory:

```powershell
Push-Location .\Build\Binaries\Debug
try {
    .\Quiet_Launcher.exe
}
finally {
    Pop-Location
}
```

Expected result: the Quiet Launcher window opens and displays its
project interface.

A fresh user profile does not need an existing project to open the
Launcher.

The Launcher supports creating a project or opening an existing project.
When launching a selected project, it resolves Quiet_Editor beside its
own executable and passes the project file path.

A successful process launch requests Launcher exit. Editor initialization
can still fail afterward; successful handoff is not proof of successful
Editor startup.

Project creation and opening are covered in the Projects guide.

## Other build selections

Use matching configure and build presets:

| Selection | Configure command | Build command |
| --- | --- | --- |
| Debug Editor | `cmake --preset debug-editor` | `cmake --build --preset build-debug-editor` |
| Debug Sandbox | `cmake --preset debug-sandbox` | `cmake --build --preset build-debug-sandbox` |
| Debug Cooker | `cmake --preset debug-cooker` | `cmake --build --preset build-debug-cooker` |
| Release Launcher | `cmake --preset release-launcher` | `cmake --build --preset build-release-launcher` |

Release executables are written under Build/Binaries/Release.

Configure presets select the build tree and options.
Build presets select an existing configured tree and executable target.

Running a build preset does not replace the initial configure step.

## Troubleshooting

### Tools work in the IDE but not PowerShell

An IDE can use bundled CMake, Ninja, and compiler tools that are absent
from the terminal's PATH.

Run the prerequisite checks in the environment used for configuration.
Either use the configured IDE toolchain or prepare a terminal with the
same tools available.

For MSVC, use its developer shell. For MinGW, ensure the selected
toolchain's compiler and supporting tools are available.

### Ninja is unavailable

If configuration reports that CMAKE_MAKE_PROGRAM is unset or Ninja
cannot be found, check:

```powershell
ninja --version
```

Make Ninja available in the configuring environment, then rerun the
configure preset.

The supplied presets explicitly select Ninja. Choosing another
generator requires a separate configuration rather than reusing the
same cached build directory with a different generator.

### The compiler cannot be found

The presets do not select a compiler.

Check the terminal environment or IDE toolchain configuration.
Keep the compiler selection consistent for an existing build tree.

When changing toolchains, use a separate configure directory rather
than mixing incompatible cached compiler settings.

### Vulkan or glslc cannot be found

The root project requires Vulkan, and shader compilation invokes
Vulkan_GLSLC_EXECUTABLE.

Ensure the Vulkan development environment is discoverable by CMake.
Then check:

```powershell
glslc --version
```

Rerun configuration after correcting discovery. A successful SDK lookup
does not establish that the installed graphics driver can run Quiet.

### Dependency downloads fail

Initial configuration uses FetchContent to retrieve third-party
dependencies.

Check the failing download's diagnostic, network access, and Git
availability. Rerun configuration once the cause is resolved.

### Fonts, textures, or shaders are missing

Run the executable from its output directory and verify the neighboring
Resources directory.

Fonts and textures are copied by post-build commands. Shader outputs
are built through Quiet_Shaders for Editor and Sandbox.

If copied resources were removed after a successful build, an otherwise
up-to-date incremental build may not rerun post-build copy commands.
Rebuild the affected target when necessary.

### Launcher cannot find the Editor

Quiet_Launcher resolves Quiet_Editor beside its own executable.

Build the Launcher target with the supplied Launcher preset and keep
both executables in the configuration's output directory.

If the Editor was moved or deleted, restore it through the build before
trying the handoff again.

### Debug Editor or Sandbox reports missing validation layers

The Vulkan instance implementation requests
VK_LAYER_KHRONOS_validation when NDEBUG is not defined and fails
initialization if that layer is unavailable.

Make the validation layer discoverable in the runtime environment.
Release builds disable this request through NDEBUG.

### Vulkan reports no suitable device

Quiet checks more than the presence of a Vulkan-capable GPU.
Device selection also requires its queue, extension, and swapchain
conditions.

Read the startup diagnostics to distinguish driver enumeration failure,
missing devices, and failure to select a suitable device.

### Windows reports a missing runtime DLL

For Windows MinGW builds, Engine's post-build commands copy
libgcc_s_seh-1.dll, libstdc++-6.dll, and libwinpthread-1.dll from the
selected compiler directory to the build output.

Keep the required runtime files with the executables. Check the
selected toolchain and copy-step diagnostics if those files are missing.

## Verification checklist

Before moving on:

- Prerequisite tools are available in the configuring environment.
- The selected configure preset completes successfully.
- Its matching build preset completes successfully.
- Launcher and Editor executables exist in the selected output directory.
- Resources contains the expected fonts, textures, and compiled shaders.
- Launching from the output directory opens the Launcher interface.

This establishes the first-launch path. Project creation, Editor use,
asset importing, scripting, and cooking are covered by the practical
guides that follow.