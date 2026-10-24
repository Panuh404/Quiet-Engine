# Application <!--! {#quiet_system_application} -->

Quiet::Core::Application coordinates the engine systems used by the Editor
and Sandbox executables.

It stores an AppSpec, creates the shared engine infrastructure, selects
mode-specific startup, and runs the application loop.

This chapter covers configuration, ownership, common startup, Editor
bootstrap, Sandbox startup, frame execution, active Scene and camera
selection, and shutdown.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Application configuration and mode | `Engine/include/Quiet/Core/AppSpec.h` |
| Application declaration and owned state | `Engine/include/Quiet/Core/Application.h` |
| Startup, frame execution, and shutdown | `Engine/src/Core/Application.cpp` |
| Staged project opening and editable startup Scene fallback | `Engine/src/Project/ProjectWorkflow.cpp` |
| Incremental project asset loading | `Engine/src/Project/ProjectAssetLoader.cpp` |
| Editor active Scene selection and runtime teardown | `Engine/src/Editor/EditorRuntimeController.cpp` |
| Editor executable configuration | `Editor/src/main.cpp` |
| Sandbox executable configuration | `Sandbox/src/main.cpp` |

See [Architecture](../Architecture.md) for the broader system boundaries
and [Scene and Actors](Scene.md) for Scene ownership and runtime contracts.

## Configuration

Quiet::Core::AppSpec supplies the initial application configuration.
Application receives it by value and moves that value into its stored
specification.

| Field | Default | Meaning |
| --- | --- | --- |
| Name | `"Quiet Application"` | Initial window title and application name used in logging |
| WindowIconPath | Empty | Icon path forwarded to window creation |
| ProjectFilePath | Empty | Optional project selection for Editor startup |
| RuntimePackagePath | Empty | Optional runtime package directory for Sandbox startup |
| Width | `1280` | Requested window width |
| Height | `720` | Requested window height |
| StartMaximized | `true` | Requests a maximized window at startup |
| Mode | `AppMode::Sandbox` | Selects Editor or Sandbox behavior |

Width and Height describe the requested window dimensions. With
StartMaximized enabled, they do not guarantee the resulting window size.

ProjectFilePath and RuntimePackagePath serve different startup paths.
A runtime package path identifies a package directory, not an editable
project descriptor.

### Modes and project selection

| Mode and input | Startup selection |
| --- | --- |
| Editor with ProjectFilePath | Uses the explicitly selected project path |
| Editor without ProjectFilePath | Requests the initial project selection from ProjectWorkflow |
| Sandbox with RuntimePackagePath | Loads the supplied runtime package |
| Sandbox without RuntimePackagePath | Initializes the last-opened or default editable project |

Sandbox's editable-project startup does not use ProjectFilePath as an
explicit project selection.

A supplied Sandbox package path is authoritative. Invalid package startup
does not fall back to an editable project.

## Ownership

Application stores ProjectContext and ProjectRegistry directly.

It owns the following objects through unique pointers:

- Window and Input.
- PhysicsSubsystem and AudioSubsystem.
- FreeCamera and the main Scene.
- VulkanRenderer.
- ProjectAssetLoader and ProjectWorkflow.
- Editor controllers and workflows when running in Editor mode.
- The separate runtime Scene used by Editor runtime transitions.

Owning a workflow does not mean that the workflow owns every object it uses.

ProjectAssetLoader receives references to the project context, renderer
asset libraries, audio library, and Vulkan resources.

ProjectWorkflow receives references to the project context, project
registry, asset loader, main Scene, and mesh library.

The renderer owns its mesh and texture libraries. The audio subsystem
owns its audio library. Application coordinates the lifetime of these
owners and the objects that use them.

## Common startup

Application construction first initializes logging, then calls
InitializeCommonStartup(), followed by mode-specific startup.

Common startup performs these operations in order:

1. Creates the Window using the specification's title, icon, dimensions,
   and maximization setting.
2. Checks whether GLFW reports Vulkan support.
3. Creates Input and requests mouse capture for Sandbox mode.
4. Creates the physics subsystem, audio subsystem, free camera, and main Scene.
5. Creates the Vulkan renderer.
6. Creates the retained demonstration Panel and Label.
7. Creates ProjectAssetLoader using the existing libraries and renderer resources.
8. Creates ProjectWorkflow using the existing project state, loader, and Scene.
9. Initializes the project registry for Editor mode, or the last-opened
   or default project for Sandbox without a package path.

Sandbox with a package path skips that final editable-project initialization.
Its package initialization happens in the mode-specific startup path.

Unavailable Vulkan support and failed required project initialization
produce exceptions. Application construction does not catch those exceptions.

### Construction and readiness

Successful Application construction does not imply that the Editor project
has finished loading.

Editor startup prepares its controllers and project selection. Project
bootstrapping proceeds through later application frames.

Some Editor selection failures are stored as a failed bootstrap state,
allowing construction to finish. Other startup failures can throw.

## Editor startup

Quiet::Core::Application::InitializeEditorStartup() creates the Editor
controller, Scene workflow, and runtime controller.

The Editor runtime controller receives the main Scene and a reference
to Application's separate runtime Scene pointer. Startup initially selects
Edit mode; it does not begin Play or Simulate.

### Project selection

When AppSpec::ProjectFilePath is nonempty, Application converts it to an
absolute, lexically normalized path and checks whether it exists.

An existence-check failure sets the bootstrap stage to Failed and records
an error message. This check alone does not establish that the selected
path contains a valid project descriptor.

Without an explicit path, Application requests the initial project path
from ProjectWorkflow. An empty result also sets the stage to Failed.

Otherwise, startup selects WaitingForFirstFrame. Project preparation,
asset loading, and startup Scene reconstruction happen during later frames.

### Bootstrap stages

The stages are declared by
Quiet::Core::Application::EditorBootstrapStage.

| Stage | Operation when bootstrap advances | Following stage |
| --- | --- | --- |
| WaitingForFirstFrame | Defers project preparation until a bootstrap frame has completed | PreparingProject |
| PreparingProject | Calls ProjectWorkflow::BeginStagedOpenProject() | LoadingAssets on success; Failed on a false result |
| LoadingAssets | Advances incremental asset loading when it is incomplete | Remains LoadingAssets until completion, then LoadingScene |
| LoadingScene | Finishes staged project opening and resets the Editor Scene session | Ready if those operations return normally |
| Ready | No further bootstrap work | Normal Editor frames |
| Failed | No further bootstrap work | Failure UI remains available |

RunEditorFrame() routes every non-Ready stage through
RunEditorBootstrapFrame().

A bootstrap frame builds the loading or failure UI and asks the renderer
to draw the main Scene. AdvanceEditorBootstrap() is called only when
DrawFrame() returns true.

Each call processes the current bootstrap stage once. A successful frame
does not necessarily complete a stage: asset loading can require multiple
advances.

If drawing does not complete, bootstrap does not advance during that frame.

### Incremental loading

ProjectWorkflow::BeginStagedOpenProject() prepares the project, updates
its recent-project entry, attempts to save the registry, and starts
incremental asset loading.

A registry-save failure produces a warning and does not by itself fail
project preparation.

ProjectAssetLoader advances through mesh, texture, and audio stages.
An advance processes at most one asset record, or moves between stages
when no records remain.

This work is called directly from the application frame. Dividing loading
across frames does not make it asynchronous or guarantee a fixed frame time.

Asset loaders that report false set the progress HadFailures flag.
Completion and success are separate conditions: the incremental sequence
can finish with reported failures.

### Scene loading and readiness

ProjectWorkflow::FinishStagedOpenProject() warns when asset loading
reported failures, then loads or creates the startup Scene.

The editable startup helper creates a default Scene when SceneDocument::Load()
returns false. Serializer exceptions are not converted into that fallback.

Application then resets the Editor Scene session and selects Ready.

Ready therefore means that the bootstrap sequence completed. It does not
guarantee that every registered asset loaded successfully.

See [Scene serialization](Scene.md) for the load-result,
partial-state, and exception contracts.

### Failure behavior

The following conditions explicitly select Failed:

- The explicitly selected project path fails the existence check.
- No initial project path is available.
- BeginStagedOpenProject() returns false.

The failure UI displays the recorded message and provides a Close Editor
button, which stops the application loop.

Failed is not a general exception boundary. Exceptions from bootstrap
operations can propagate through Run(); Application does not automatically
convert them into the failure UI.

Closing through that UI is ordinary loop termination. Run() returns zero,
so its return value does not independently report bootstrap success.

## Sandbox startup

Quiet::Core::Application::InitializeSandboxStartup() completes Sandbox
initialization before Application construction returns.

There are two input paths: a runtime package directory, or the editable
project initialized during common startup.

### Runtime package startup

When AppSpec::RuntimePackagePath is nonempty, startup performs these
operations in order:

1. Converts the package root to an absolute, lexically normalized path
   and requires it to be a directory.
2. Loads RuntimeManifest.json from that directory.
3. Initializes ProjectContext for the runtime package.
4. Loads built-in runtime support assets.
5. Creates a temporary RuntimeAssetLoader using the existing renderer
   and audio libraries.
6. Loads runtime mesh, texture, and audio assets.
7. Resolves the manifest's StartupScene against the package root and
   requires the result to be a regular file.
8. Loads that Scene through SceneDocument using the loaded mesh library.

The temporary RuntimeAssetLoader is an orchestration object. Loaded assets
remain in the renderer and audio libraries after it leaves scope.

A failed required check or a false required load result produces an
exception. Exceptions from called operations can also propagate.

A supplied package is authoritative: package failure does not trigger
editable-project fallback or default Scene creation.

### Editable-project startup

When RuntimePackagePath is empty, common startup has already initialized
the last-opened or default editable project and its startup Scene.

InitializeSandboxStartup() skips package loading and continues with that
main Scene. ProjectFilePath is not used here as an explicit project selection.

### Entering Play

After either input path, Application obtains the initial swapchain extent.
When both dimensions are nonzero, it requests a viewport resize.

It then calls BeginRuntime() on the main Scene with RuntimeMode::Play,
passing the physics subsystem, audio subsystem, project context, and Input.

Sandbox therefore begins Play during construction. It does not wait for
the first Run() frame, and it does not create the Editor's separate runtime
Scene for this startup path.

Loading Scene data and beginning runtime are distinct operations.
SceneDocument::Load() reconstructs supported Scene data; BeginRuntime()
establishes the runtime worlds and behavior.

## Startup comparison

| Contract | Editor | Sandbox |
| --- | --- | --- |
| Loading schedule | Staged through bootstrap frames | Completed during construction |
| Initial Scene mode | Edit | Play |
| Runtime Scene ownership | Separate Scene pointer available for Editor transitions | Runs the main Scene |
| Explicit input | Editable project descriptor path | Runtime package directory |
| Selected startup failures | Recorded Failed state with failure UI | Exceptions |
| Asset-loading completion | Can reach Ready with reported asset failures | Required package loader false results throw |
| Default Scene fallback | Editable startup helper can use it after a false Scene load result | Editable-project path can use it; package path does not |

## Frame execution

Quiet::Core::Application::Run() initializes its previous-time sample,
then loops while m_Running is true and the Window has not requested closure.

Each iteration:

1. Calls BeginFrame().
2. Executes the Editor or Sandbox frame path.
3. Calls FinalizePlatformWindows() with the frame result.

The loop conditions are checked at the beginning of each iteration.
A stop request made during an iteration does not automatically skip the
remaining operations in that iteration.

Run() returns zero after ordinary loop termination. It does not catch
execution exceptions.

### Beginning a frame

BeginFrame() performs these operations in order:

1. Polls Window events.
2. Updates Input.
3. Processes global shortcuts.
4. Calculates elapsed time.
5. Applies pending viewport resizing unless deferred.
6. Returns a FrameContext containing delta time and the ImGui layer pointer.

CalculateDeltaTime() subtracts the previous glfwGetTime() sample from the
current sample, converts the difference to float, and updates the previous
sample.

Application does not clamp this delta or implement fixed stepping here.
Any subsystem-specific stepping belongs to the Scene runtime and its systems.

In Editor mode, holding the left mouse button defers the pending viewport
resize application performed by BeginFrame().

### Ready Editor frames

RunEditorFrame() requires an Editor controller and a UI layer.
If either is missing, it logs an error and returns false.

Before bootstrap reaches Ready, it executes the bootstrap frame path
described in the Editor startup section.

Once Ready, the frame performs these operations:

1. Processes Editor requests.
2. Resolves the active Scene.
3. Updates Editor navigation and that Scene.
4. Begins the UI frame.
5. Builds and processes Editor UI, viewport asset drops, and resize requests.
6. Ends the UI frame.
7. Resolves the active Scene again.
8. Builds render camera data, updates the retained UI paint list, and
   submits the Scene and commands to the renderer.

The second Scene lookup is significant. UI processing can trigger a
runtime transition, so the Scene selected for rendering may differ from
the Scene updated earlier in the same frame.

Do not assume that one cached active Scene reference remains appropriate
across an Editor runtime transition.

### Scene updates by mode

UpdateEditorSceneForFrame() first handles viewport navigation and the
configured free-camera movement speed.

Edit mode can pilot a selected camera Actor. Otherwise, Edit and Simulate
can use free-camera navigation.

| Frame mode | Runtime input call | Scene update |
| --- | --- | --- |
| Editor Edit | Not called | Update(deltaTime) |
| Editor Simulate | HandleRuntimeInput() before Update() | Update(deltaTime) |
| Editor Play | HandleRuntimeInput() before Update() | Update(deltaTime) |
| Sandbox | HandleRuntimeInput() when runtime is active | Update(deltaTime) |

Calling HandleRuntimeInput() does not imply identical input behavior
across runtime modes. The Scene's runtime contract determines what it does.

### Sandbox frames

RunSandboxFrame() requires a UI layer. A missing layer produces an error
log and a false frame result.

Sandbox uses the main Scene directly. It handles runtime input when active,
updates the Scene, and processes viewport resizing.

It then builds the runtime presentation UI, selects render camera data,
updates the retained UI paint list, and submits the Scene and commands
without the Editor grid or selection feedback.

## Retained runtime UI ownership and submission

Application stores Quiet::UI::Widget m_RuntimeUIRoot and
Quiet::UI::PaintList m_RuntimeUIPaintList directly.

InitializeCommonStartup() creates the demonstration Panel and its child
Label after constructing VulkanRenderer. Their explicit bounds and text
are defined in that function.

The hierarchy belongs to Application, independently of the editable Scene
and disposable Editor runtime Scene. Starting or stopping Play does not
clone, replace, or reset it. It currently has no Scene persistence,
Inspector authoring, or Lua integration.

BuildRuntimeUIPaintList() runs before rendering ready Editor and Sandbox
frames. It performs these operations:

1. Reads the actual Scene target extent from VulkanRenderer.
2. Updates the root bounds to that extent at position (0, 0).
3. Clears the command list and returns for a zero extent.
4. Reuses the existing list when the root's Paint flag is clear.
5. Otherwise clears the list, traverses the hierarchy through Paint(),
   and clears Paint invalidation across the root subtree.

An unchanged root extent does not invalidate the hierarchy. Bounds changes
invalidate layout, which also marks paint and interaction dirty.

Only Paint invalidation is acknowledged here. Layout and Interaction
flags remain available for future systems; Application does not currently
perform automatic layout or route widget input.

RunEditorFrame() and RunSandboxFrame() pass a span over the cached list
to DrawFrame(). The renderer borrows it during that call and retains no
command references. Editor bootstrap frames use the default empty span.

Widgets and commands own no GPU resources. Image commands retain texture
IDs; the renderer resolves those IDs and manages submission texture
lifetimes separately. The Application tree can therefore remain alive
after ShutdownCommon() explicitly releases the renderer.

See [Renderer](Renderer.md) for hierarchy contracts, invalidation,
ordered painting, image descriptors, and current limitations.

## Active Scene and render camera

Application::GetActiveScene() returns the main Scene outside Editor mode,
or when no Editor runtime controller exists.

Otherwise, it delegates to EditorRuntimeController::GetActiveScene().

| Editor state | Selected Scene |
| --- | --- |
| Edit | Editable main Scene |
| Non-Edit with an existing runtime Scene | Runtime Scene |
| Non-Edit without a runtime Scene | Editable main Scene fallback |

Normal Play and Simulate transitions create a separate runtime Scene.
The controller's fallback does not create that Scene or begin runtime.

### Render camera selection

GetPreferredRenderCameraActor() looks for a primary camera Actor with a
CameraComponent.

Sandbox uses that camera when available. Editor uses it only in Play mode.

Otherwise, BuildActiveRenderCameraData() uses the free camera.

| Mode | Preferred render camera |
| --- | --- |
| Editor Edit | Free camera |
| Editor Simulate | Free camera |
| Editor Play | Primary Scene camera, otherwise free camera |
| Sandbox | Primary Scene camera, otherwise free camera |

Edit-mode camera piloting updates the selected camera Actor and synchronizes
the free-camera pilot state. It does not change the render-camera selection
rule to the Play-mode rule.

Render aspect ratio comes from the Scene viewport extent. A zero width or
height is replaced with 1 for this calculation.

Editor grid rendering requires ShowGrid and a mode other than Play.
Selection feedback similarly requires ShowSelectionFeedback and a mode
other than Play.

## Frame completion

The Editor and Sandbox frame functions return the renderer's DrawFrame()
result after their update and UI work.

A false result does not automatically terminate Run(), undo Scene updates,
or produce a nonzero application return value.

FinalizePlatformWindows() updates and renders additional ImGui platform
windows only when:

- The frame result is true.
- ImGui's ViewportsEnable configuration flag is enabled.

Editor bootstrap advancement also requires a true draw result.
Normal Scene updates occur before drawing and are not gated by that result.

## Shutdown

Application destruction calls ShutdownCommon(), then Log::Shutdown().

Ordinary termination of Run() does not itself destroy the Application.
The executable entry points destroy their local Application after Run()
returns.

### Runtime termination

ShutdownCommon() first asks the Editor runtime controller to shut down,
when present.

That controller ends active runtime on its runtime Scene and releases it.

Application then ends runtime on the main Scene if it is active.
This covers Sandbox runtime before the main Scene is destroyed.

Runtime termination happens while the physics and audio subsystems still
exist, allowing Scene runtime teardown to use its dependencies.

### Resource release order

After runtime termination, ShutdownCommon() resets its owned pointers
in this order:

1. EditorRuntimeController.
2. EditorSceneWorkflow.
3. ProjectWorkflow.
4. ProjectAssetLoader.
5. EditorController.
6. Runtime Scene.
7. Main Scene.
8. AudioSubsystem.
9. PhysicsSubsystem.
10. FreeCamera.
11. VulkanRenderer.
12. Input.
13. Window.

The runtime Scene reset can be redundant because Editor runtime shutdown
may already have released it.

Workflows are released before the Scene and libraries they reference.
Scenes are released before the runtime subsystems and renderer.
Input is released before its Window.

Logging remains available during this shutdown sequence and is shut down
afterward.

### Construction failure

The explicit destructor sequence applies to a fully constructed Application.

If construction throws, Application's destructor is not called.
Already-constructed members are unwound by C++, but this is not equivalent
to executing ShutdownCommon() and Log::Shutdown().

Sandbox's entry-point exception handlers explicitly shut down logging.
The Editor entry point has no corresponding handler.

## Executable configuration

Editor/src/main.cpp selects Editor mode, sets the title to "Quiet Editor",
and supplies the Editor icon path.

When a nonempty first argument is supplied, the entry point converts it
to an absolute, lexically normalized ProjectFilePath.

Sandbox/src/main.cpp selects Sandbox mode, sets the title to "Quiet Sandbox",
and requests a 1200 by 800 window. StartMaximized retains its default value.

When a first argument is supplied, Sandbox stores it as RuntimePackagePath.

The Sandbox entry point catches exceptions from Application construction
and execution, reports the failure, shuts down logging, and returns 1.
The Editor entry point has no equivalent exception handler.