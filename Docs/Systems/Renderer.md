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
| Panel rendering | PanelPipeline |
| Image rendering | ImagePipeline and ImageResources |
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

This resource is independent of ImGui's font atlas. It supplies glyph
coverage for Actor-authored text and retained Label commands, but does
not provide project font-asset lookup.

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
descriptor, sets a positive-height viewport, and uses TextRenderContext::Scissor
when supplied, otherwise the full target. The caller must provide a valid
target-contained scissor. It retains no references between calls.

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

The Actor text pass runs after Scene, selection, and ordered UI rendering,
before the Scene image transitions to shader-read layout. It synchronizes
preceding color writes and loads the existing color attachment for blending.
No depth attachment is used.

Temporary layouts remain alive while TextRenderSystem records their
draws. The render system retains no references afterward. No component
stores a font resource, GPU handle, or layout cache.

This path is shared by Editor and Sandbox presentation. Editor runtime
testing renders the disposable runtime Scene supplied by Application.

## Retained UI and ordered painting

Quiet::UI provides a retained widget hierarchy and backend-independent
painting commands. Application owns the hierarchy and cached command list.
VulkanRenderer borrows that list during DrawFrame() and records the
corresponding Vulkan operations.

This path is separate from Actor-authored TextComponent and UIRectComponent
data. Widgets have no Scene serialization, Inspector controls, or Lua API.
They do not replace the Editor's Dear ImGui tooling.

### Implementation map

| Responsibility | Source and principal symbols |
| --- | --- |
| Pixel-space geometry | `Engine/include/Quiet/UI/Geometry.h`: Quiet::UI::Rect |
| Hierarchy, bounds, invalidation, and traversal | `Engine/include/Quiet/UI/Widget.h` and `Engine/src/UI/Widget.cpp`: Quiet::UI::Widget |
| Borrowed measurement resources | `Engine/include/Quiet/UI/LayoutContext.h`: Quiet::UI::LayoutContext |
| Vertical container | `Engine/include/Quiet/UI/VerticalBox.h` and `Engine/src/UI/VerticalBox.cpp`: Quiet::UI::VerticalBox |
| Horizontal container | `Engine/include/Quiet/UI/HorizontalBox.h` and `Engine/src/UI/HorizontalBox.cpp`: Quiet::UI::HorizontalBox |
| Solid background widget | `Engine/include/Quiet/UI/Panel.h` and `Engine/src/UI/Panel.cpp`: Quiet::UI::Panel |
| Text widget | `Engine/include/Quiet/UI/Label.h` and `Engine/src/UI/Label.cpp`: Quiet::UI::Label |
| Image widget | `Engine/include/Quiet/UI/Image.h` and `Engine/src/UI/Image.cpp`: Quiet::UI::Image |
| Button state painting and behavior | `Engine/include/Quiet/UI/Button.h` and `Engine/src/UI/Button.cpp`: Quiet::UI::Button |
| Owned logical commands | `Engine/include/Quiet/UI/PaintCommands.h`: SolidRectCommand, TextCommand, ImageCommand, PaintCommand, PaintList |
| Tree ownership, layout, and paint caching | `Engine/src/Core/Application.cpp`: InitializeCommonStartup(), UpdateRuntimeUILayout(), and BuildRuntimeUIPaintList() |
| GPU command translation | `Engine/src/Renderer/Vulkan/VulkanRenderer.cpp`: DrawFrame() and RecordFrameCommandBuffer() |
| Rectangle graphics state | `Engine/src/Renderer/Vulkan/Panel/PanelPipeline.h`: PanelPipeline |
| Image graphics state | `Engine/src/Renderer/Vulkan/Image/ImagePipeline.h`: ImagePipeline |
| Image descriptors and texture retention | `Engine/src/Renderer/Vulkan/Image/ImageResources.h`: ImageResources |
| Shader data | `Engine/src/Renderer/Vulkan/RenderTypes.h`: PanelPushConstants and ImagePushConstants |
| Image shaders | `Engine/Resources/Shaders/Image.vert` and `Image.frag` |

### Hierarchy and geometry

Widget owns its children through unique pointers and stores a borrowed
parent pointer. Widgets cannot be copied or moved. AddChild() transfers
ownership, rejects null children and cycles, and returns the attached
widget. DetachChild() transfers ownership of a direct child back to the
caller, clears its parent pointer, and returns null when it is not found.
Attachment and detachment clear the transferred root's previous arrangement
and invalidate layout. Authored geometry and descendant ownership survive.

GetChild() uses bounds-checked access. References identify separately
allocated widget objects, so growing the child vector does not move those
objects. References remain usable only while the widget remains alive.

Rect uses pixels with positive X rightward and positive Y downward.
Rect itself is geometry data: a zero dimension is empty. Widget
separately stores authored bounds, measured desired size, and an optional
parent-assigned arrangement. SetBounds() supplies an authored pixel offset
and nonnegative size request. It rejects nonfinite values and negative
dimensions without changing the previous request. GetLocalBounds() returns
that authored request, even after a container arranges the widget.

SetAnchorAndPivot() rejects nonfinite inputs and clamps finite coordinates
to [0, 1]. With no parent-assigned arrangement, surface placement resolves as:

```text
position = parent.position + authored.offset
           + anchor * parent.size - pivot * widget.size
```

A parentless widget ignores anchor and pivot. SetArrangedBounds() assigns a
validated parent-relative rectangle without overwriting authored bounds,
anchor, or pivot. An arrangement takes precedence and bypasses anchor/pivot
resolution. GetSurfaceBounds() adds the parent's surface position to this
rectangle. ClearArrangedBounds() restores authored placement until another
arrangement is assigned. Changing authored inputs does not immediately
remove an existing arrangement; callers must process pending layout before
using geometry. Resolved positions and maximum coordinates must remain finite.

Paint() calls the widget's OnPaint() first, then visits children in
insertion order. The hierarchy and paint-affecting state must remain
unchanged during traversal. Without an enabled clip, empty parent bounds do
not prevent descendants from painting. Clipping is an explicit setting,
described below.

### Measurement, arrangement, and alignment

Widget::Measure() recursively measures children before calling OnMeasure()
and storing GetDesiredSize(). Results must be finite and nonnegative;
invalid results throw std::runtime_error. Measurement exceptions propagate
and the traversal does not roll back earlier child results. Measure() does
not assign geometry or acknowledge invalidation. The base OnMeasure()
returns authored size and does not infer a size from child extents.

ArrangeChildren() calls OnArrangeChildren() before descending into children.
Measure() must have completed after the latest layout-input change. The base
arranger gives children their measured sizes and resolves authored offsets,
anchors, and pivots against the allocated parent size. Containers override
that placement policy; authored child geometry remains available for reuse.

LayoutContext borrows CPU resources only during measurement. Application
supplies VulkanRenderer::GetDefaultFontAtlas(), the same renderer-owned CPU
atlas used by default text rendering. Widgets retain neither this pointer
nor GPU font resources. Label::OnMeasure() calls TextLayout::BuildUTF8() at
the Label's pixel height and takes the component-wise maximum of logical
text size and authored size. Empty content uses authored size. Nonempty
content without a font pointer throws; unsuccessful text layout falls back
to authored size. The existing text layout generates temporary glyph quads
as well as metrics; this is reused for consistency, not a separate metrics
cache. No wrapping or per-line text alignment is performed.

VerticalBox, HorizontalBox, and Button use authored size as minimum preferred
dimensions. Their zero authored dimensions permit content-driven sizing;
they are not a universal auto-size flag for every Widget subclass.

| Container | Desired size from content | Arrangement |
| --- | --- | --- |
| VerticalBox | Maximum child width, sum of child heights, inter-child spacing, and padding | Top to bottom; retain desired heights; horizontal alignment within available width |
| HorizontalBox | Sum of child widths, maximum child height, inter-child spacing, and padding | Left to right; retain desired widths; vertical alignment within available height |
| Button | Component-wise maximum child size plus padding | Children share the padded content area, using both alignment axes |

Padding uses left, top, right, bottom in XYZW. Padding and spacing setters
reject nonfinite or negative values. Spacing occurs only between children.
Button padding defaults to 16 pixels on every side; box padding and spacing
default to zero. Padding subtraction clamps available dimensions to zero.

Widget::SetAlignment() stores horizontal and vertical Alignment values,
defaulting to Stretch. Start keeps desired size at the allocation's minimum;
Center offsets by half the remaining space; End offsets by all remaining
space; Stretch uses the available dimension. Boxes apply only the secondary
axis shown in the table. Base Widget placement still uses anchors and pivots.
Button applies alignment independently on both axes. Alignment does not
change measurement, authored geometry, or allocate proportional space.
Oversized non-stretched content keeps its desired size; Center or End may
therefore produce negative local offsets. Clipping controls its visibility.

Application processes layout before valid pointer hit tests and before
painting. A dirty root triggers measurement and arrangement of the complete
tree; there is no per-widget measurement cache that skips clean descendants.
See [Application](Application.md) for the exact frame integration.

### Widget clipping

SetClipToBounds() defaults to false. When enabled, this widget and its
descendants are restricted to its surface rectangle. GetSurfaceClip()
intersects enabled clips on this widget and its ancestor chain. No value
means no widget clip; a zero-area intersection means no visible region.
This is evaluated from current geometry without storing an ancestor pointer
or a clip cache. It changes neither measurement nor arrangement.

Paint() suppresses an empty clipped branch. Otherwise it stamps the
effective Clip value onto commands appended by this widget's OnPaint(),
then visits children. OnPaint() must leave Clip unset and only append its
own operations. HitTest() uses the same resolved intersection before
searching children, including when queried directly on a subtree. Minimum
edges are included and maximum edges excluded. Hidden overflow cannot
acquire hover or receive release-inside activation; an existing press
continues under the normal ownership and cancellation rules.

### Widgets and invalidation

Panel emits a SolidRectCommand for positive bounds. Label emits a
TextCommand when its content is nonempty and both bounds dimensions are
positive. Its text starts at the arranged surface-space top-left. Aligning
a Label moves its logical rectangle, not individual text lines. Bounds do
not wrap or scale text; enabled widget clips restrict its visible output.

Image emits an ImageCommand when its texture ID is nonempty and both
dimensions are positive. It stretches the complete texture over its
bounds without preserving aspect ratio or inferring natural dimensions.
Setting the ID neither loads nor validates a texture.

Panel color, Label color, and Image tint reject nonfinite input and clamp
finite channels to [0, 1]. Label pixel height must be finite and positive.
Content remains owned by Label; UTF-8 validation occurs during text layout.

Each widget initially has Layout, Paint, and Interaction invalidation
flags set. Invalidate() marks the widget and its ancestors:

| Reason | Flags set |
| --- | --- |
| Layout | Layout, Paint, Interaction |
| Paint | Paint |
| Interaction | Interaction |

Bounds changes, attachment, detachment, Label content changes, and Label
height changes invalidate layout. Anchors, pivots, alignment, padding, and
spacing changes also invalidate layout. Color, tint, and image-ID changes
invalidate paint. Arrangement changes and clip toggles invalidate Paint and
Interaction without requesting another layout. Setters avoid invalidation
when the effective value is unchanged.

ClearInvalidation() clears only the requested flag on the widget and its
descendants; it does not clear ancestor flags. Application acknowledges
Layout after complete measurement and arrangement, and Paint after command
generation. Interaction remains unacknowledged by the current pointer path:
hit tests and state updates are recomputed each frame without an interaction
cache.

### Hit testing and pointer state

Widget::HitTest() receives surface-space pixels and searches children in
reverse insertion order before considering the widget itself. This reverses
painting order so the topmost participating widget wins. Parent bounds
restrict descendants only when clipping is enabled. A zero-area enabled
clip excludes the branch; otherwise zero parent size does not prevent hits.

HitTestMode controls participation without changing painting:

| Mode | Behavior |
| --- | --- |
| SelfAndChildren | Search descendants, then this widget's bounds |
| ChildrenOnly | Search descendants and skip this widget; the default |
| None | Exclude the entire subtree |

Self hits require positive dimensions, include minimum edges, and exclude
maximum edges. Nonfinite points return null. Returned pointers are borrowed;
queries do not extend lifetime. Bounds must satisfy the same finite resolved
coordinate requirements as painting.

UpdatePointerHover() marks only the directly hit widget, not its ancestors.
UpdatePointerPress() starts primary-button ownership on the directly hit
widget, preserves it while held outside, and identifies activation only on
release over that same widget. It returns the borrowed activation target
after updating the subtree; it does not invoke behavior during traversal.
All false button values cancel without activation. None cancels the subtree.

Hover and press transitions invalidate Paint only when the stored boolean
changes. Attachment and detachment clear both states on the transferred
subtree. No previous-widget pointer is retained between frames.

Application supplies current-frame mapping, cancellation, and gameplay
consumption. See [Application](Application.md) for presentation snapshots,
coordinate spaces, execution order, and input-policy limits.

### Button behavior and callback lifetime

Button derives directly from Widget and selects SelfAndChildren in its
constructor. Its child Label normally remains ChildrenOnly, so text does
not block the Button. Children paint after its background using the existing
rectangle and text commands; no additional Vulkan pipeline is needed.

The current straight-alpha linear RGBA defaults are:

| Visual condition | Background |
| --- | --- |
| Not hovered, including dragging an owned press outside | (0.03, 0.12, 0.25, 0.8) |
| Hovered without a press | (0.08, 0.22, 0.42, 1.0) |
| Hovered with an owned press | (0.02, 0.08, 0.18, 1.0) |

SetOnClick() installs a std::function<void()> in a shared owning allocation;
an empty function clears it. Activate() retains that allocation locally and
invokes the same callable, preserving mutable capture state between calls.
It does not copy the callable on each activation. Installation allocation
and callback exceptions propagate.

The local owner keeps an executing callback alive if it replaces itself or
removes its Button. Activate() accesses no Button members after invocation.
Captured references must still remain valid; retaining the callable does
not extend the lifetime of objects referenced by those captures. Invoke
behavior outside hierarchy traversal. Widget::Activate() defaults to no work.

This is primary-button state and release-inside activation, not a general
captured pointer-event dispatcher. Keyboard/gamepad focus, configurable
styles, and additional pointer event types remain future work.

### Logical commands and ownership

PaintCommand is a variant of SolidRectCommand, TextCommand, and
ImageCommand. PaintList owns its commands and strings. Neither type
depends on Vulkan or ImGui.

SolidRectCommand owns surface-space Min/Max coordinates and straight-alpha
linear RGBA Color. TextCommand owns UTF-8 Content, surface-space Origin,
positive PixelHeight, and Color. It uses the renderer's default runtime
font. ImageCommand owns Min/Max, a texture-library TextureID, and Tint.
Project textures use their existing asset IDs. Each operation also owns an
optional surface-space Rect Clip. No clip permits the full target; an empty
clip suppresses drawing. Clip geometry and its maximum coordinates must be
finite, with nonnegative size.

Coordinates must be finite; rectangles require Max greater than Min on
both axes. Color and tint channels must be finite and within [0, 1].
These are producer requirements, not general validation performed by the
Vulkan consumer. Text layout rejects malformed UTF-8 and invalid heights;
failed or empty layouts produce no glyph draws.

Commands retain no widgets, textures, fonts, descriptors, or GPU handles.
DrawFrame() borrows their storage only for the duration of the call.
Each temporary text-layout result owns its glyph geometry until recording
returns. GPU resource lifetime is handled separately.

Application reuses its cached PaintList until Paint invalidation requests
a rebuild. This caches logical commands, not Vulkan command buffers or
glyph geometry: text layout and Vulkan recording still happen each frame.

### Ordering and rendering

The consumer executes commands in vector order without sorting by type.
Later operations blend above earlier operations where geometry overlaps.
Each operation binds its required pipeline and descriptors.

The ordered pass runs after Scene and selection rendering. It synchronizes
preceding color writes and uses one color-only LOAD/STORE rendering scope,
a positive-height viewport, and a per-command scissor. Each command's clip
is intersected with the target extent before conversion to VkRect2D. Empty
intersections skip drawing; unclipped commands restore the full-target
scissor. Minimum edges round down and maximum edges round up to integer
pixels. Raster clipping can therefore include a boundary pixel outside the
exact floating-point rectangle used for hit testing.

Text commands pass this scissor through TextRenderContext::Scissor, so
TextRenderSystem does not replace it with the full target. Actor-authored
text omits that optional value and retains its full-target scissor. Image
clipping preserves the original geometry and UVs: it crops the image rather
than stretching the visible remainder. No pipeline or descriptor lifetime
changes are required by clipping.

Rectangle and image commands each draw six generated vertices. Text
commands pass layout geometry to TextRenderSystem. Missing image texture
IDs skip drawing without loading a resource. IDs are resolved during frame
recording, so later frames can use replacement library resources without
rebuilding the cached PaintList.

Actor-authored text remains a separate compatibility pass afterward and
therefore paints above the entire retained list. There is no global
interleaving of Actor text and widget commands.

The final Scene color transition to SHADER_READ_ONLY_OPTIMAL remains
after both UI passes, before ImGui samples the Scene image.

### Image descriptors and GPU lifetime

ImagePipeline owns its descriptor-set layout, pipeline layout, and
graphics pipeline. It borrows the logical device.

ImageResources borrows the device and descriptor-set layout. It owns a
reusable descriptor pool and shared texture references for the current
submission. Each image draw receives a distinct descriptor set; sets are
not repeatedly overwritten for different draws.

After waiting for the previous frame fence, DrawFrame() counts image
commands and calls BeginFrame() before resetting the fence. That count
provides an upper bound on descriptor capacity, including commands whose
texture IDs may be unavailable.

BeginFrame() grows the pool when required or resets the existing pool.
It retires old descriptor sets before releasing retained textures.
CreateBinding() writes a sampled-texture descriptor and retains its
texture until the next BeginFrame() or destruction.

This lifetime model depends on the renderer's current single in-flight
submission. Multiple frames in flight would require revisiting descriptor
and texture-retention ownership.

### Shader and pipeline contracts

PanelPushConstants contains Rect, Color, and Viewport at byte offsets
0, 16, and 32. ImagePushConstants uses Rect, Tint, and Viewport at the same
offsets. Both structures occupy 48 bytes and use vertex-stage push ranges.

Rect stores pixel-space minimum and maximum XY. Viewport stores positive
target dimensions in XY, with ZW reserved. Vertex shaders generate two
triangles using gl_VertexIndex and firstVertex=0, independently of the
Scene camera.

Panel.frag outputs color directly. Image.frag samples a combined image
sampler at set 0, binding 0, using full-texture UV coordinates, and
multiplies the sample by Tint.

Both pipelines use color-only dynamic rendering, one sample, no vertex
input, no face culling, no depth testing or writing, and dynamic viewport
and scissor. RGB blending uses source alpha and one minus source alpha;
alpha blending uses one and one minus source alpha.

Pipeline construction releases partially created resources on failure.
Destruction requires completed GPU use and a valid logical device.

Image resources are released before ImagePipeline during shutdown and
swapchain recreation. Recreation waits for device idle and rebuilds both
for the replacement pipeline layout and target format. Ordinary viewport
extent changes use dynamic viewport/scissor without rebuilding pipelines.

### Current demonstration and limitations

InitializeCommonStartup() in `Engine/src/Core/Application.cpp` creates a
Panel at (24, 24), sized 320 by 120 pixels, and a child Label with authored
offset (16, 24) and minimum size 288 by 72. Its text is "Retained UI label";
its surface origin is (40, 48).

A VerticalBox has authored offset (-24, -24), minimum width 320, and anchor
and pivot (1, 1). Its bottom-right corner therefore remains 24 pixels from
the surface edges. It uses 12-pixel padding and spacing. A centered heading,
"Retained actions" at pixel height 24, sits above a HorizontalBox with a
12-pixel gap. Its Buttons contain "First action" and
"Second action\nMeasured text". Label measurements and Button padding drive
their sizes. The first Button centers vertically beside the taller second
Button. Each child Label uses centered rectangle alignment.

The first release-inside callback logs "First retained action activated.";
the second logs "Second retained action activated.". Label changes invalidate
layout upward, so the row, outer column, and anchored position can be
recomputed without hardcoded button heights.

A separate clipping Panel at (24, 180), sized 180 by 80, owns a Button at
local offset (120, 20) with minimum size 160 by 64 and text "Clipped action".
Only the part within the Panel is visible and interactive. Its callback
logs "Clipped retained action activated.". This deliberate overflow is a
clipping demonstration, not truncated content caused by a font failure.

This Application-owned tree appears in ready Editor frames and Sandbox.
It survives Editor Play and Stop because it is not owned by either Scene.
The temporary image preview has been removed; Image remains available as
a C++ widget.

Measurement is unconstrained. Minimum/maximum size policies, proportional
space allocation, wrapping, scrolling, and automatic shrinking are not
implemented. A small viewport can overflow the anchored group; clipping
does not reflow it. Image has no intrinsic texture-size measurement.
Keyboard/gamepad focus, style resolution, widget serialization, and a UI
designer remain future work. The complete Text and runtime UI roadmap area
is not finished by these layout foundations.

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
3. Prepares image descriptor capacity and retires previous image bindings.
4. Resets the fence.
5. Acquires a swapchain image.
6. Updates camera resources for that image.
7. Records its command buffer.
8. Submits graphics work with the in-flight fence.
9. Requests presentation.
10. Returns true when the normal path succeeds.

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
| 5 | Scene color | Execute caller-supplied UI commands in submission order |
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

Recreation clears the Scene texture registration and releases image
resources before the image pipeline, followed by panel and text pipelines,
dependent Editor effects, the Scene target, UI layer, synchronization,
graphics commands, mesh pipeline, material resources, camera resources,
and depth.

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

1. ImageResources and ImagePipeline, in that order.
2. FrameSync and StaticMeshRenderSystem.
3. Selection outline render system and pipeline.
4. Selection mask render system and pipeline.
5. Editor grid render system and pipeline.
6. GraphicsCommands.
7. StaticMeshPipeline and StaticMeshMaterialResources.
8. CameraResources.
9. SceneViewportTarget.
10. ImGuiLayer.
11. DepthResources.
12. MeshLibrary, TextureLibrary, PanelPipeline, TextRenderSystem,
    TextPipeline, and the default runtime font texture.
13. Swapchain.
14. LogicalDevice and PhysicalDevice.
15. VulkanSurface and VulkanInstance.

This is the explicit teardown path for a fully constructed renderer.
Member unwinding after failed construction follows C++ destruction rules
instead.

External shared pointers do not keep the logical device alive. Consumers
must release device-dependent resources within the device's valid lifetime.
