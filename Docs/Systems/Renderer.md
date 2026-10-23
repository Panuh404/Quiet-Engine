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
| Solid panel pipeline | `Engine/src/Renderer/Vulkan/Panel/PanelPipeline.h` |
| Screen-space shader data | `Engine/src/Renderer/Vulkan/RenderTypes.h` |
| Backend-independent paint commands | `Engine/include/Quiet/UI/PaintCommands.h` |
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

VulkanRenderer owns Vulkan resource wrappers through unique pointers.
It also stores the default runtime font's CPU atlas by value.

| Group | Owned objects |
| --- | --- |
| Vulkan context | VulkanInstance, VulkanSurface, PhysicalDevice, LogicalDevice |
| Presentation | Swapchain and DepthResources |
| Scene viewport | SceneViewportTarget |
| Frame infrastructure | GraphicsCommands, CameraResources, FrameSync |
| Asset libraries | MeshLibrary and TextureLibrary |
| Runtime font | Default FontAtlasData and its Texture2D coverage texture |
| Text rendering | TextPipeline and TextRenderSystem |
| Panel preview | PanelPipeline |
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

### Default runtime font

CreateRenderResources() imports Resources/Fonts/Inter-Regular.ttf at a
32-pixel ascender-to-descender height and uploads its coverage through
Texture2D. The path is relative to QUIET_ENGINE_RESOURCES_PATH.

VulkanRenderer owns the CPU atlas by value and the GPU texture through
a unique pointer. Import failure throws with the font path and importer
error. Texture creation failures propagate from Texture2D.

The font is created in both Editor and Sandbox modes. It survives Scene
viewport resizing and swapchain recreation because its uploaded texture
depends on the retained logical device.

DestroyRenderResources() releases the font texture after TextureLibrary
and before Swapchain and LogicalDevice. Member declaration order also
keeps the logical device alive while the font texture is released during
construction failure unwinding.

This resource is independent of ImGui's font atlas. It does not yet
submit text draws or provide project font-asset lookup.

### UI layer

VulkanRenderer owns ImGuiLayer and exposes it by reference.

Application uses that layer to build Editor, bootstrap, or runtime UI
before passing it to DrawFrame().

The renderer and Application therefore coordinate different parts of
the same UI frame lifecycle.

## CPU text layout

Quiet::Renderer::TextLayout::Build() converts Unicode codepoints and a
successful FontAtlasData result into owned TextGlyphQuad values.

Build() accepts Unicode codepoints through std::u32string_view.
BuildUTF8() accepts UTF-8 bytes through std::string_view, decodes them
into temporary codepoint storage, and delegates layout to Build().

Malformed UTF-8 returns an error with a zero-based byte offset and no
geometry. Overlong encodings, surrogate codepoints, truncated sequences,
invalid continuation bytes, and values above U+10FFFF are rejected.
A byte-order mark is treated as a codepoint rather than stripped.

Valid unsupported codepoints use the font atlas's '?' fallback once per
codepoint. UTF-8 decoding does not expand the atlas's ASCII coverage.
Scene text rendering uses BuildUTF8().

Positions are relative to a top-left origin, with positive Y pointing
down. The first baseline uses the scaled font ascent. Subsequent lines
advance by the scaled LineHeight. Kerning and glyph advances use the
requested pixel height.

LF, CRLF, and standalone CR create line breaks. Tabs advance by four
spaces rather than to tab stops. Kerning does not cross a line break
or tab. Spaces can advance the pen without producing drawable geometry.

Size reports logical line-box dimensions, including trailing spaces
and empty lines. It is not a tight visible-pixel bounding box. Empty
text produces zero dimensions and no quads; a trailing line break
creates an additional empty line.

Layout results retain no references to the font or input string.
This stage performs no wrapping, alignment, clipping, or GPU drawing.

## Text shader interface

TextPushConstants describes one glyph using four aligned vectors:
pixel-space rectangle, atlas UV rectangle, straight-alpha linear color,
and target dimensions. Its C++ size is 64 bytes, with members at offsets
0, 16, 32, and 48.

Text.vert generates two triangles from six vertices using gl_VertexIndex.
Its intended draw uses vertexCount=6 and firstVertex=0, without vertex
buffers. Positions use a top-left pixel origin and require positive
target dimensions and a positive-height Vulkan viewport.

The vertex shader converts pixels directly to normalized device
coordinates. It does not use the Scene camera or its projection Y-flip.

Text.frag samples font coverage from the red channel at descriptor set 0,
binding 0, using a combined image sampler. Coverage multiplies output
alpha while RGB remains unmultiplied. The intended pipeline uses
straight-alpha blending with depth testing and writing disabled.

TextPipeline creates the graphics pipeline, pipeline layout, descriptor-set
layout, descriptor pool, and font descriptor set. It uses color-only
dynamic rendering with one sample, straight-alpha blending, no vertex
inputs, and disabled depth testing, depth writing, and face culling.
Viewport and scissor are dynamic.

The push-constant range is visible to the vertex stage. The font sampler
is visible to the fragment stage. The supplied R8_UNORM font texture is
borrowed and must outlive the pipeline's use.

VulkanRenderer creates the text pipeline after the default font texture.
Swapchain recreation destroys and rebuilds the pipeline while retaining
the font texture. Normal teardown destroys the pipeline before that
texture and the logical device.

TextPipeline releases partially created Vulkan resources if construction
throws. Destruction requires outstanding GPU use to have finished.

TextRenderSystem records one six-vertex draw per visible glyph using
vertex-stage push constants. It binds the text pipeline and font
descriptor, sets a positive-height viewport, and clips to the full target
through a scissor rectangle. It retains no references between calls.

VulkanRenderer reads TextComponent values from the Scene supplied to
DrawFrame(). It processes Actors in Scene::GetActors() order, using the
default font atlas and each component's content, pixel height, color,
and visibility.

The fixed text preview has been removed. Text Actors with UIRectComponent
resolve their origin as:

origin = anchor * target extent + offset - pivot * resolved size

Target extent is the actual Scene render target size, not the requested
viewport size or swapchain size. Each positive UIRectComponent::Size
axis supplies an explicit dimension; each zero axis uses layout.Size on
that axis. Natural dimensions include trailing spaces and empty lines,
rather than tight ink bounds. Negative or nonfinite Size values skip
that Actor's text. Text starts at the resolved rectangle's top-left;
explicit bounds do not scale, wrap, clip, or align text within it.

Text without UIRectComponent retains origin (24, 24). Default rectangle
values, including Size (0, 0), produce the same origin. Placement is
recalculated each frame, and nonfinite resolved origins skip that Actor's
text.

Actor world transforms and Editor hierarchy folders do not affect this
screen-space placement. Inspector exposes Offset, Size, Anchor, and Pivot
through the separate UI Rectangle component section.

Layout is rebuilt each frame for visible, nonempty text. Unsuccessful
layouts and layouts without visible glyphs are skipped. Layout errors
are not currently surfaced in the Inspector or logged per frame.

The Actor text pass runs after Scene, selection, and ordered-preview rendering,
before the Scene image transitions to shader-read layout. It synchronizes
preceding color writes and loads the existing color attachment for blending.
No depth attachment is used.

Temporary layouts remain alive while TextRenderSystem records their
draws. The render system retains no references afterward. No component
stores a font resource, GPU handle, or layout cache.

This path is shared by Editor and Sandbox presentation. Editor runtime
testing renders the disposable runtime Scene supplied by Application.

## Ordered UI painting preview

Quiet::UI provides backend-independent rectangle and text command data.
The current producer and Vulkan consumer both remain local to
VulkanRenderer::RecordFrameCommandBuffer(). This establishes a painting
data boundary, not a retained widget hierarchy or a general submission API.

The preview is separate from Scene data. There is no panel component,
serialization member, Inspector section, or panel Lua API.
UIRectComponent does not control this preview.

Implementation anchors are:

- Engine/include/Quiet/UI/PaintCommands.h: SolidRectCommand, TextCommand,
  PaintCommand, and PaintList in Quiet::UI.
- Engine/src/Renderer/Vulkan/RenderTypes.h: PanelPushConstants.
- Engine/Resources/Shaders/Panel.vert and Panel.frag: pixel geometry and color.
- Engine/src/Renderer/Vulkan/Panel/PanelPipeline.h and PanelPipeline.cpp:
  PanelPipeline ownership and graphics state.
- Engine/src/Renderer/Vulkan/VulkanRenderer.cpp:
  CreateRenderResources(), RecreateSwapchain(), DestroyRenderResources(),
  and the paintList producer/consumer in RecordFrameCommandBuffer().

### Logical commands and ownership

SolidRectCommand owns resolved surface-space Min/Max coordinates and
straight-alpha linear RGBA Color. TextCommand owns UTF-8 Content,
surface-space Origin, PixelHeight, and the same color representation.
Surface coordinates currently use Scene-target pixels, with top-left
origin, positive X rightward, and positive Y downward.

PaintCommand is a std::variant of these two types. PaintList is an owning
std::vector of commands. The header has no Vulkan or ImGui dependency.
Its structures are logical values, not shader-memory layouts; the backend
explicitly translates rectangles into PanelPushConstants and text into
TextLayoutResult and TextRenderContext.

Commands specify finite coordinates and finite color channels within
[0, 1]. Rectangles require Max greater than Min on each axis; text requires
finite positive PixelHeight. These are caller requirements, not a general
validation service: the current rectangle consumer does not validate
those values. Text layout rejects malformed UTF-8 and invalid heights,
and the consumer skips failed or empty layouts.

Text uses the renderer's default runtime font. Commands retain no font
resources, texture references, GPU handles, or widget pointers. The local
list owns its strings throughout recording. Each local TextLayoutResult
owns its glyph geometry until TextRenderSystem::Record() returns; the
render system retains none of it. Vulkan pipeline and font ownership
remain with VulkanRenderer.

The list is currently rebuilt each frame, reserves two elements, and
performs text layout during consumption. The Actor compatibility path
also copies content into an owned TextCommand. There is no persistent
paint cache, glyph batching, or allocation-reuse policy yet.

### Ordering contract

The consumer visits commands in vector order without sorting by type.
Later commands blend over earlier operations where their geometry
overlaps; an opaque later rectangle can cover earlier text. Each command
binds the appropriate graphics pipeline, so text/rectangle alternation
preserves the submitted order.

This guarantee applies within PaintList. Actor-authored text remains a
separate compatibility pass afterward and therefore paints above the
entire preview list. That path still resolves Actor UI rectangles and
uses TextCommand values before recording through TextRenderContext.
There is no global interleaving of Actors and preview commands.

### Shader and pipeline contract

PanelPushConstants contains Rect, Color, and Viewport at byte offsets
0, 16, and 32, with total size 48 bytes. Rect stores pixel-space minimum
and maximum XY; Color is straight-alpha linear RGBA; Viewport stores
positive target dimensions in XY, with ZW reserved.

Panel.vert generates two triangles from gl_VertexIndex using six vertices
and firstVertex=0. Its pixel-to-device conversion uses a top-left origin
and a positive-height viewport, without the Scene camera. Panel.frag
outputs the interpolated color directly. Neither shader uses descriptors.

PanelPipeline owns a pipeline layout and graphics pipeline and borrows
the logical device. It uses color-only dynamic rendering, one sample,
no vertex input, no face culling, no depth testing or writing, and dynamic
viewport/scissor. Color blending uses source alpha and one minus source
alpha; alpha blending uses one and one minus source alpha.

The 48-byte push-constant range is vertex-stage only. Temporary
ShaderModule objects are destroyed after pipeline creation. Constructor
failures release partially created resources. Destruction releases the
pipeline before its layout and requires completed GPU use.

VulkanRenderer owns the pipeline in both Editor and Sandbox modes.
Startup constructs it with the Scene target color format. Swapchain
recreation waits for device idle, destroys it, and rebuilds it for the
replacement target format. Shutdown waits for idle and destroys it before
the logical device. Ordinary Scene target extent changes use dynamic
viewport/scissor without rebuilding the pipeline.

### Recording and visible behavior

The local PaintList contains, in order:

- A rectangle from (24, 24) to (344, 144), giving a 320 by 120 pixel panel,
  with linear RGBA (0.03, 0.12, 0.25, 0.8).
- White text "Paint order preview" at (40, 48), with PixelHeight 32,
  using the renderer's default font.

The preview block runs when the Scene target is valid, its extent is
nonzero, and either the panel pipeline or the text pipeline/render system
is available. Individual commands skip when their required rendering
objects are unavailable.

The pass runs after selection overlays and before Actor-authored text.
It synchronizes preceding color writes, begins one color-only LOAD/STORE
rendering scope, and sets a positive-height viewport and full-target
scissor. Rectangle commands translate their values into push constants
and record vkCmdDraw(6, 1, 0, 0). Text commands build layout and forward
geometry to TextRenderSystem, which binds its pipeline and font descriptor
and records glyph draws. It also sets the same viewport/scissor extent.

The existing Actor text-pass barrier synchronizes preview writes before
that pass blends its text above the preview.
The final Scene color transition to SHADER_READ_ONLY_OPTIMAL remains
after both passes, before ImGui samples the Scene image.

The preview is independent of Actors and appears during Edit, Play, and
Simulate. Its pixel placement remains fixed when the viewport resizes;
the full-target scissor clips portions outside smaller targets. It has
no interaction, hit testing, parenting, per-command clipping, or authored
overlap controls. The list supports ordering, but the preview's producer
currently hardcodes that order. Retained widget producers remain future
work and do not replace the Editor's Dear ImGui tooling.

To verify ordering during development, temporarily submit the text before
the rectangle and make the rectangle opaque. Expected result: the
rectangle covers the preview text. Restore rectangle-first ordering and
alpha 0.8 afterward. Actor-authored text still renders above both.

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
| 5 | Scene color | Execute the preview PaintList in submission order |
| 6 | Scene color | Blend visible Actor-authored text |
| 7 | Swapchain image | Clear and render ImGui |

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

Recreation clears the Scene texture registration and releases text and
panel pipelines, dependent Editor effects, the Scene target, UI layer,
synchronization, graphics commands, mesh pipeline, material resources,
camera resources, and depth.

It moves the old swapchain into a local owning pointer and supplies its
handle when constructing the replacement.

The renderer rebuilds the dependent resources for the new swapchain.
Editor grid and selection resources are recreated only in Editor mode.

MeshLibrary, TextureLibrary, the default font atlas/texture, and the
stateless TextRenderSystem are retained. Vulkan instance, surface,
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
11. MeshLibrary, TextureLibrary, PanelPipeline, TextRenderSystem,
    TextPipeline, and the default runtime font texture.
12. Swapchain.
13. LogicalDevice and PhysicalDevice.
14. VulkanSurface and VulkanInstance.

This is the explicit teardown path for a fully constructed renderer.
Member unwinding after failed construction follows C++ destruction rules
instead.

External shared pointers do not keep the logical device alive. Consumers
must release device-dependent resources within the device's valid lifetime.