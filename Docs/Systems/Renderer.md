# Renderer <!--! {#quiet_system_renderer} -->

Quiet::Renderer::Vulkan::VulkanRenderer owns the Vulkan rendering
infrastructure used by Application.

It coordinates resource creation, frame rendering, presentation,
Scene viewport targets, asset libraries, and renderer-side Editor effects.

Application supplies the Scene, camera data, completed UI frame, and
Editor rendering options.

This chapter covers responsibilities, ownership, synchronization,
frame results, command recording, render passes, viewport resizing,
swapchain recreation, and teardown.

See [Application](Application.md) for frame orchestration and camera
selection, and [Assets](Assets.md) for loading and library contracts.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Renderer declaration and owned state | `Engine/src/Renderer/Vulkan/VulkanRenderer.h` |
| Resource creation, frame rendering, resizing, and teardown | `Engine/src/Renderer/Vulkan/VulkanRenderer.cpp` |
| Vulkan instance | `Engine/src/Renderer/Vulkan/Context/VulkanInstance.h` |
| Window surface | `Engine/src/Renderer/Vulkan/Context/VulkanSurface.h` |
| Physical device selection | `Engine/src/Renderer/Vulkan/Context/PhysicalDevice.h` |
| Logical device and queues | `Engine/src/Renderer/Vulkan/Context/LogicalDevice.h` |
| Graphics command resources | `Engine/src/Renderer/Vulkan/Context/GraphicsCommands.h` |
| Swapchain | `Engine/src/Renderer/Vulkan/Frame/Swapchain.h` |
| Frame synchronization | `Engine/src/Renderer/Vulkan/Frame/FrameSync.h` |
| Camera resources | `Engine/src/Renderer/Vulkan/Frame/CameraResources.h` |
| Depth resources | `Engine/src/Renderer/Vulkan/Frame/DepthResources.h` |
| Scene viewport target | `Engine/src/Renderer/Vulkan/Frame/SceneViewportTarget.h` |
| Mesh rendering | `Engine/src/Renderer/Vulkan/Mesh/StaticMeshRenderSystem.h` |
| Editor grid recording | `Engine/src/Renderer/Vulkan/Editor/EditorGridRenderSystem.cpp` |
| Selection mask recording | `Engine/src/Renderer/Vulkan/Editor/SelectionMaskRenderSystem.cpp` |
| Selection outline recording | `Engine/src/Renderer/Vulkan/Editor/SelectionOutlineRenderSystem.cpp` |
| Material resources | `Engine/src/Renderer/Vulkan/Mesh/StaticMeshMaterialResources.h` |
| UI integration | `Engine/src/Renderer/UI/ImGuiLayer.h` |

## Responsibilities

Application constructs VulkanRenderer after creating the Window,
Input, runtime subsystems, free camera, and main Scene.

The renderer stores a borrowed Window pointer and an Editor-mode flag
derived from AppSpec.

Construction calls CreateRenderResources(). Destruction calls
DestroyRenderResources().

Copy construction and copy assignment are explicitly disabled.

### Frame inputs

See the DrawFrame sequence and result sections for synchronization,
submission, presentation, and failure behavior.

Application selects the active Scene and camera before calling it.
The renderer does not choose Editor Play or Simulate transitions.

The Scene and other borrowed inputs must remain valid while the call
uses them.

## Owned resource groups

VulkanRenderer stores its rendering objects through unique pointers.

| Group | Owned objects |
| --- | --- |
| Vulkan context | VulkanInstance, VulkanSurface, PhysicalDevice, LogicalDevice |
| Presentation | Swapchain and DepthResources |
| Scene viewport | SceneViewportTarget |
| Frame infrastructure | GraphicsCommands, CameraResources, FrameSync |
| Asset libraries | MeshLibrary and TextureLibrary |
| Mesh rendering | StaticMeshPipeline, StaticMeshRenderSystem, StaticMeshMaterialResources |
| Editor grid | EditorGridPipeline and EditorGridRenderSystem |
| Selection feedback | SelectionMask and SelectionOutline pipelines and render systems |
| UI | ImGuiLayer |

These groups explain responsibilities. They are not the exact construction
or destruction order.

Resource wrappers can borrow handles and references from other owned
objects. Their lifetimes therefore depend on more than pointer ownership.

## Ownership boundaries

### Window

Application owns the Window. VulkanRenderer borrows it.

The Window must outlive the renderer's use of its surface and window state.
Application's shutdown releases the renderer before Input and Window.

### Scene

Application owns the editable or Sandbox Scene and the Editor runtime
Scene pointer.

DrawFrame() receives the selected Scene by reference. VulkanRenderer does
not take ownership of that Scene.

### Libraries

The renderer owns MeshLibrary and TextureLibrary.

Their getters return references to existing libraries, not ownership.
Project loaders borrow these references.

Meshes and textures stored in those libraries use shared pointers.
An external shared pointer can extend a resource object's lifetime.

That extension does not keep the renderer's Vulkan device alive.
Resource lifetime must remain compatible with the device and other Vulkan
dependencies used by the resource.

### UI layer

VulkanRenderer owns ImGuiLayer and exposes it by reference.

Application uses that layer to build Editor, bootstrap, or runtime UI
before passing it to DrawFrame().

The renderer and Application therefore coordinate different parts of
the same UI frame lifecycle.

## Swapchain and Scene viewport

The renderer distinguishes presentation extent from Scene viewport extent.

GetSwapchainExtent() describes the presentation target.
GetSceneViewportExtent() describes the Scene target.

Application uses the Scene viewport extent for render camera aspect ratio.

Editor viewport dimensions can differ from the main Window's presentation
dimensions. Sandbox requests Scene viewport sizing from the swapchain extent.

RequestViewportResize() and ApplyPendingViewportResize() form a deferred
resize interface. See Deferred Scene viewport resizing for timing and
readiness behavior.

GetSceneViewportTextureID() exposes the Scene image for UI display.
Do not assume that a returned texture identifier remains usable across
target recreation; follow the target and UI lifetime contracts.

## Construction and teardown boundaries

Successful construction establishes the renderer's owned infrastructure.
Failures can propagate to Application startup.

For a fully constructed renderer, destruction enters
DestroyRenderResources(), which requests device idle before releasing
owned rendering resources.

The teardown keeps the logical device alive while dependent owned resources
are released, then releases device, surface, and instance infrastructure.

If construction throws, VulkanRenderer's destructor is not called.
Already-constructed members unwind through C++; that is distinct from
executing the explicit teardown sequence.

See Device idle and teardown for the explicit release order and
synchronization-result handling.

## Frame synchronization

Quiet::Renderer::Vulkan::FrameSync owns:

- One image-available semaphore.
- One in-flight fence.
- One render-finished semaphore per swapchain image.

The in-flight fence is initially signaled, allowing the first frame's
wait to complete without an earlier submission.

DrawFrame() waits for the preceding graphics submission through this
single fence before preparing the next submission.

Render-finished semaphores are indexed by acquired swapchain image.
They are not selected using a separate rotating frame counter.

FrameSync stores the logical device handle. The device must outlive
destruction of its synchronization objects.

## DrawFrame sequence

DrawFrame() performs these operations in order:

1. Checks the Window's framebuffer resize flag.
2. Waits for the in-flight fence.
3. Resets the fence.
4. Acquires a swapchain image.
5. Updates camera resources for that image.
6. Records its command buffer.
7. Submits graphics work with the in-flight fence.
8. Requests presentation.
9. Returns true when the normal path succeeds.

A framebuffer resize detected at entry calls RecreateSwapchain() and
returns false before fence waiting or submission.

### Fence waiting and reset

The fence wait uses UINT64_MAX as its timeout.
A failed wait or reset throws std::runtime_error.

Reset occurs before image acquisition and command recording.

The function is not an exception-safe retry boundary. If a later operation
throws before successful submission, the reset fence is not automatically
signaled or replaced by this function.

Callers must not assume that catching an exception and immediately calling
DrawFrame() again restores a valid synchronization state.

### Image acquisition

vkAcquireNextImageKHR() uses the image-available semaphore and UINT64_MAX.

| Acquisition result | Behavior |
| --- | --- |
| VK_SUCCESS | Continues |
| VK_SUBOPTIMAL_KHR | Continues |
| VK_ERROR_OUT_OF_DATE_KHR | Recreates the swapchain and returns false |
| Other result | Throws std::runtime_error |

The out-of-date path enters swapchain recreation, which replaces FrameSync.
It does not continue using the reset fence for a normal submission.

### Graphics submission

The graphics submission contains one recorded command buffer.

It waits on the image-available semaphore at
VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.

It signals the render-finished semaphore selected by imageIndex and
associates the submission with the in-flight fence.

A failed vkQueueSubmit() throws std::runtime_error.

### Presentation

Presentation uses the acquired image index and waits on its render-finished
semaphore.

| Presentation condition | Behavior |
| --- | --- |
| VK_ERROR_OUT_OF_DATE_KHR | Recreates the swapchain and returns false |
| VK_SUBOPTIMAL_KHR | Recreates the swapchain and returns false |
| Window reports framebuffer resizing | Recreates the swapchain and returns false |
| Other non-success result, without those conditions | Throws std::runtime_error |
| VK_SUCCESS, without resizing | Returns true |

The recreation conditions are checked before the remaining presentation
error check.

## DrawFrame result contract

| Outcome | Meaning |
| --- | --- |
| True | The normal acquisition, recording, submission, and presentation-call path succeeded |
| False before submission | A resize or out-of-date acquisition triggered recreation |
| False after submission | Presentation conditions triggered recreation |
| Exception | An operation failed without completing a normal return path |

True does not report GPU completion or guarantee that the frame has already
appeared on screen.

False is not a rollback result. Application has already performed its
Scene update and UI work before calling DrawFrame(), and graphics work
may already have been submitted.

Swapchain recreation can itself throw or wait for a nonzero framebuffer
size. A branch intended to return false does not guarantee an immediate
or successful return.

### Application integration

Application uses the result to decide whether to update and render
additional ImGui platform windows.

Editor bootstrap also advances only after a true result.

Ordinary Scene updates are performed before drawing and are not undone
when DrawFrame() returns false.

See [Application](Application.md) for the surrounding frame loop.

## Command recording

RecordFrameCommandBuffer() obtains the command buffer associated with
the acquired swapchain image.

It resets that buffer, begins recording, records the rendering passes,
and ends recording.

vkBeginCommandBuffer() and vkEndCommandBuffer() failures throw.
The vkResetCommandBuffer() result is not checked in this implementation.

Recording commands does not execute them. DrawFrame() submits the recorded
buffer afterward.

## Render pass order

The implementation uses dynamic rendering scopes.

| Order | Target | Operation |
| --- | --- | --- |
| 1 | Scene color and depth | Clear and render static meshes |
| 2 | Same Scene attachments | Optionally render the Editor grid |
| 3 | Selection mask and Scene depth | Clear the mask and record selected geometry |
| 4 | Scene color | Optionally composite the selection outline |
| 5 | Swapchain image | Clear and render ImGui |

The Scene passes require an existing, valid SceneViewportTarget.
When that condition is false, the Scene block is skipped, but swapchain
UI recording still proceeds.

## Scene rendering

Before rendering, the Scene color image transitions to
COLOR_ATTACHMENT_OPTIMAL and depth transitions to
DEPTH_ATTACHMENT_OPTIMAL.

The renderer clears Scene color to (0.05, 0.07, 0.12, 1.0) and depth to 1.0.
Both attachments use STORE after rendering.

StaticMeshRenderSystem receives a context containing the command buffer,
image index, mesh pipeline, material resources, camera resources,
texture library, Scene, and Scene viewport extent.

The renderer delegates mesh draw recording to that system.

### Editor grid

The grid records after meshes within the same rendering scope.

It requires drawEditorGrid to be true and both the grid pipeline and
render system to exist.

Application determines whether to request the grid from Editor state.
The renderer applies the resource-presence checks.

## Selection feedback

The optional selected Actor UUID is resolved against the supplied Scene.
An absent or unresolved UUID leaves selectedEntity null.

### Selection mask

In Editor mode, when the mask pipeline and render system exist, the renderer
records a mask pass even when no Actor is selected.

It synchronizes Scene depth for selection reading, clears the mask to zero,
and loads the existing Scene depth instead of clearing it.

SelectionMaskRenderSystem skips drawing when:

- The selected entity is null or invalid.
- The extent is zero.
- StaticMesh or Transform is missing.
- The mesh component is invalid or hidden.
- The mesh is missing or has no indices.

When drawing proceeds, the system records an indexed draw using the
selected mesh's complete index count and Actor transform.

The mask pass then transitions its image to SHADER_READ_ONLY_OPTIMAL.

Clearing the mask without selected geometry prevents the previous selection
mask from being retained as the current selection.

### Selection outline

The outline requires a valid, visible selected StaticMeshComponent,
Editor mode, an outline pipeline and render system, and a ready mask
descriptor.

The renderer loads the existing Scene color and records an overlay without
a depth attachment.

SelectionOutlineRenderSystem samples the mask through its descriptor and
records a three-vertex draw.

The mask and outline are distinct operations. Recording the mask pass
does not guarantee that outline drawing is requested.

## Scene image sampling

After Scene rendering and optional selection feedback, the Scene color
image transitions to SHADER_READ_ONLY_OPTIMAL.

The renderer then sets m_SceneViewportReady to true.

This flag is updated during command recording, before command-buffer
completion, queue submission, or GPU completion.

IsSceneViewportReady() is therefore a renderer readiness flag, not a GPU
completion signal.

## Swapchain UI rendering

The acquired swapchain image transitions from its tracked layout to
COLOR_ATTACHMENT_OPTIMAL.

The renderer clears it to (0.10, 0.10, 0.10, 1.0) and calls
ImGuiLayer::Render() inside a rendering scope with no depth attachment.

After UI recording, the image transitions to PRESENT_SRC_KHR.

Once command-buffer recording ends successfully, the swapchain's tracked
layout is updated to PRESENT_SRC_KHR.

The Scene is displayed through the UI's Scene texture usage rather than
a separate Scene-to-swapchain copy in this function.

## Recorded state and failure

Scene target layout fields are updated as transition commands are recorded.
The readiness flag is also updated during recording.

These CPU-side values describe the renderer's recorded state. They do not
independently prove that the commands executed successfully.

An exception during recording does not roll back those fields or restore
DrawFrame()'s synchronization state.

This distinction matters when reasoning about recovery: command recording,
submission, and GPU execution are separate stages.

## Deferred Scene viewport resizing

RequestViewportResize() stores the requested dimensions and marks resizing
pending.

Changing either requested dimension resets the stability counter.
Repeating the same dimensions does not reset it.

The request does not immediately recreate the target.

### Applying a request

ApplyPendingViewportResize() delegates to EnsureSceneViewportSize().

| Condition | Behavior |
| --- | --- |
| No pending request | Returns |
| Either requested dimension is zero | Returns, retaining the pending request |
| Requested dimensions already match the target | Clears the request and counter |
| Different dimensions, fewer than three eligible calls | Increments the counter and returns |
| Third eligible call | Performs target resizing |

The counter counts eligible application calls, not completed frames.
Application can defer or invoke those calls through its frame and resize
paths, so the threshold is not an exact three-frame delay.

### Target replacement

When the threshold is reached, the renderer:

1. Requests device idle.
2. Clears the ImGui Scene texture registration.
3. Resizes SceneViewportTarget.
4. Updates the selection outline's mask image binding when applicable.
5. Clears the pending request and stability counter.
6. Sets Scene viewport readiness to false.

The vkDeviceWaitIdle() result is not checked in this path.

The next valid Scene recording sets readiness true again. Until then,
GetSceneViewportTextureID() returns an empty identifier.

Exceptions during resizing propagate without restoring the previous
target or pending-state fields.

## Scene texture availability

GetSceneViewportTextureID() returns an empty ImTextureID when:

- The UI layer is absent.
- The Scene target is absent or invalid.
- Scene viewport readiness is false.

Otherwise, it obtains or creates the UI texture registration using the
target's sampler, color image view, and shader-read layout.

A texture ID is not an ownership handle. Do not retain it across target
resizing, registration clearing, or UI-layer recreation.

GetSceneViewportExtent() returns zero dimensions when no valid target
exists.

## Swapchain recreation

RecreateSwapchain() first obtains the Window framebuffer dimensions.

While either dimension is zero, it waits for Window events and checks again.
The method contains no separate Window-close check in that loop.

Recreation can therefore block while the framebuffer remains zero-sized.

It then waits for device idle. Unlike viewport resizing and WaitIdle(),
this wait result is checked; failure throws.

### Rebuilt resources

Recreation clears the Scene texture registration and releases dependent
Editor effects, the Scene target, UI layer, synchronization, graphics
commands, mesh pipeline, material resources, camera resources, and depth.

It moves the old swapchain into a local owning pointer and supplies its
handle when constructing the replacement.

The renderer rebuilds the dependent resources for the new swapchain.
Editor grid and selection resources are recreated only in Editor mode.

MeshLibrary and TextureLibrary are retained. Vulkan instance, surface,
physical device, and logical device are also retained.

After successful rebuilding, it:

- Sets Scene viewport readiness to false.
- Clears pending viewport dimensions, resize state, and stability count.
- Releases the old swapchain.
- Resets the Window framebuffer resize flag.

A previous pending Scene viewport resize request is not preserved.
Application must supply subsequent sizing requests as needed.

### Borrowed references

Recreation replaces ImGuiLayer, GraphicsCommands, and other dependent
objects.

References and pointers to those replaced objects must not be reused.
A DrawFrame() call can trigger recreation before returning false.

ProjectAssetLoader stores renderer resource pointers.
Application refreshes those bindings through
RefreshProjectAssetLoaderRendererBindings() before processing Editor
requests.

Consumers calling renderer or loader methods directly must account for
the same dependency lifetime.

### Failure behavior

Recreation destroys some old dependent resources before all replacements
have been created.

Exceptions propagate without restoring the prior renderer configuration.
The operation does not provide an atomic replacement or automatic retry
guarantee.

## Device idle and teardown

WaitIdle() calls vkDeviceWaitIdle() when a logical device exists.
It does not check or report the Vulkan result.

DestroyRenderResources() also requests device idle without checking the
result, then releases owned pointers in this order:

1. FrameSync and StaticMeshRenderSystem.
2. Selection outline render system and pipeline.
3. Selection mask render system and pipeline.
4. Editor grid render system and pipeline.
5. GraphicsCommands.
6. StaticMeshPipeline and StaticMeshMaterialResources.
7. CameraResources.
8. SceneViewportTarget.
9. ImGuiLayer.
10. DepthResources.
11. MeshLibrary and TextureLibrary.
12. Swapchain.
13. LogicalDevice and PhysicalDevice.
14. VulkanSurface and VulkanInstance.

This is the explicit teardown path for a fully constructed renderer.
Member unwinding after failed construction follows C++ destruction rules
instead.

External shared pointers do not keep the logical device alive. Consumers
must release device-dependent resources within the device's valid lifetime.