# Editor Guide <!--! {#quiet_guide_editor} -->

This guide covers Scene editing, viewport navigation, Actor components,
saving, and testing through Play and Simulate.

## Prerequisites

Build Quiet_Editor and open an editable project using
[Getting Started](../GettingStarted.md) and [Projects](Projects.md).

Wait for project loading to complete before editing.

## Main panels

| Panel | Purpose |
| --- | --- |
| Scene | Actor and folder hierarchy |
| Inspector | Selected Actor properties and components |
| Viewport | Scene viewing, selection, and transform gizmos |
| Scene Settings | Scene-level rendering, physics, and audio settings |
| Console | Engine messages and diagnostics |
| Debug | Runtime and rendering diagnostics |

Use View → Reset Layout to restore the default arrangement.

## Select and inspect an Actor

1. Select an Actor in the Scene panel.
2. Inspect its components in Inspector.
3. Edit its Transform fields while in Edit mode.

Viewport selection is also available by left-clicking a pickable mesh
or displayed debug icon. Clicking empty space clears selection.

Selection is suppressed during navigation capture, dragging assets,
and gizmo interaction.

An empty Actor has no rendered mesh, so select it through the hierarchy.

## Create and modify Actors

Use Scene → Create Empty Actor from the main menu.

The Scene panel's creation menu also provides Empty Actor, Directional
Light, Folder, and available primitive or imported-mesh choices.

Folders organize the hierarchy; they are not rendered mesh Actors.

Select an Actor and use Inspector's + Add Component button to choose an
available component. Where supported, the component menu provides
Remove Component.

Component availability and required dependencies vary by type.
Use the Inspector's validation and feedback rather than assuming every
component combination produces a runtime resource.

Ctrl+D duplicates the selected Actor.
Delete removes the selected Actor.

These Actor shortcuts do not operate on Scene selection while the Asset
Browser has keyboard focus.

## Author text

1. Create or select an Actor.
2. Choose Inspector → + Add Component → Text.
3. Expand the Text section and edit Content.
4. Use Enter to insert line breaks.
5. Click outside the content field to finish the history interaction.

Content updates appear in the Viewport while editing. Use the Text
component's options menu to remove it. Addition, removal, and completed
content interactions participate in undo/redo and dirty tracking.

Save the Scene to persist the text. Authoring is disabled during Play
and Simulate.

Text uses the default font. An optional UI Rectangle component controls
offset, dimensions, anchor, and pivot. Without it, text retains origin
(24, 24).

The atlas currently renders printable ASCII; other valid Unicode
codepoints display '?'.
Visible toggles rendering. Pixel Height controls text size from 1 to 256.
Color edits linear RGBA, including opacity. Size and color update while
editing and participate in undo/redo when the interaction finishes.
Visibility changes record an immediate history operation.

Larger text scales the existing bitmap atlas rather than rerasterizing
the font. Use UI Rectangle to control screen-space placement.

## Position text

Select an Actor with Text, then choose + Add Component → UI Rectangle.

- Offset moves the text in pixels.
- Size defines rectangle dimensions in pixels. Zero on either axis uses
  the text's natural logical dimension on that axis.
- Anchor selects a normalized point in the Scene viewport.
- Pivot selects the corresponding point in the resolved rectangle.

For centered text, use Size (0, 0), Offset (0, 0), Anchor (0.5, 0.5),
and Pivot (0.5, 0.5).

For bottom-right text placement with a 24-pixel margin, use Size (0, 0),
Offset (-24, -24), Anchor (1, 1), and Pivot (1, 1).

For an explicit 300 by 80 rectangle centered in the viewport, use Size
(300, 80), Offset (0, 0), and Anchor/Pivot (0.5, 0.5). Text starts at
that rectangle's top-left, 150 pixels left and 40 pixels above viewport
center. Explicit dimensions do not scale, wrap, clip, or align text
within the rectangle. Negative dimensions entered through the Inspector
are clamped to zero.

Placement updates while editing. Finish the interaction before testing
Scene undo/redo. Save the Scene to persist the rectangle.

Removing UI Rectangle preserves Text and restores its (24, 24) fallback.
The rectangle does not create visible content by itself.

### Retained UI demonstration

Application currently creates a retained Panel and child Label independently
of Actor components. The translucent dark-blue panel measures 320 by 120
pixels at Scene viewport position (24, 24). White text "Retained UI label"
appears at (40, 48), using the default runtime font at pixel height 32.

The demonstration appears in Edit, Play, and Simulate. Its hierarchy belongs
to Application and survives Play and Stop. Editing an Actor's Text or UI
Rectangle does not change these widgets.

Placement uses fixed pixels. Resizing preserves their offsets, while the
Scene target edges clip drawing outside the viewport. Parent bounds do not
clip child content. Actor-authored text renders after retained widgets.

A Button measuring 320 by 64 pixels appears at (24, 168), with the current
label "Hover me". It becomes brighter while hovered and darker while
pressed inside. Release over the same button logs
"Retained UI button activated." once. The text is demonstration content;
the button supports activation despite its current label.

To exercise pointer ownership, start with mouse capture disabled. In Play,
release capture using Shift+F1. Press inside, drag outside, and return:
the normal background appears outside and the pressed background returns
inside. Release outside cancels activation. A press originating outside
does not activate by dragging in. Camera capture cancels a pending press.

In Edit mode, clicking participating runtime UI skips Actor click selection,
so an existing Actor selection is preserved. Normal selection outside the
interactive widgets remains available. This guard does not disable gizmos.

In Simulate or Play, UI-owned left-button gestures are hidden from gameplay
mouse-button queries after routing. When testing with a script that logs
left-button presses, expect an activation message for a UI click and a
gameplay message for an outside click. Repeat after resizing or detaching
the Viewport; visible and interactive bounds should remain aligned.

There are no widget Inspector controls, automatic layout, or keyboard/gamepad
focus yet. The temporary logo preview is removed; the Image widget and its
renderer remain available through C++.

See [Application](../Systems/Application.md) for ownership and paint-list
caching, and [Renderer](../Systems/Renderer.md) for widget and GPU contracts.

## Navigate the Viewport

Begin mouse navigation over the Viewport image.

| Input | Operation |
| --- | --- |
| Hold right mouse button and move mouse | Fly-camera look |
| W / S while flying | Forward / backward |
| A / D while flying | Left / right |
| Q / E while flying | Down / up |
| Space while flying | Up |
| Shift while flying | Movement-speed boost |
| Alt + left mouse drag | Orbit |
| Middle mouse drag | Pan |
| F with an Actor selected | Focus selected Actor |

Release the navigation mouse button to end capture.
Losing window focus also ends navigation capture.

Use the viewport camera-speed control to adjust movement speed.

## Transform gizmos

Select an Actor in Edit mode, then use the viewport toolbar or shortcuts:

| Shortcut | Operation |
| --- | --- |
| T | Translate |
| R | Rotate |
| S | Scale |
| L | Toggle Local / World for translation and rotation |
| G | Toggle snapping |

Scale does not use the Local / World toggle.

Drag the gizmo handles to edit the selected transform.
Rotation fields use degrees.

Gizmo shortcuts are suppressed during text input, UI keyboard capture,
mouse capture, or active viewport navigation.

## Undo and redo

Ctrl+Z undoes recorded Scene changes.
Ctrl+Y or Ctrl+Shift+Z redoes them.

History operates in Edit mode. Loading or replacing a Scene resets the
Scene editing session and history.

Scene history is not a universal undo mechanism for project files,
asset operations, or runtime activity.

## Save and reload a Scene

Use File → Save Scene or Ctrl+S to save to the active Scene path.

Expected result: saving succeeds, the Scene's saved state is updated,
and the Console reports the destination.

File → Load Scene or Ctrl+O reloads that active path. This action is not
an arbitrary-file picker.

File → New Scene or Ctrl+N creates a default Scene and resets the active
path to the project's startup Scene path. A later save writes there.

Save work before creating or loading another Scene. These actions can
replace the current in-memory Scene.

Scene creation, saving, and loading are available in Edit mode.

## Play and Simulate

Both modes run a copy of the editable Scene.

| Mode | Start / stop shortcut | Viewing and input |
| --- | --- | --- |
| Simulate | F6 | Editor viewing and navigation |
| Play | F5 | Runtime camera selection and mouse capture |

Use the matching shortcut again, or the Scene menu's stop action,
to return to Edit mode. Stop the current mode before starting the other.

Both modes initialize physics, audio, and scripting. Simulate is not
limited to physics alone.

Runtime changes affect the copy and are discarded when the mode stops.
They are not automatically applied to the editable Scene.

Inspector authoring and normal Scene mutation actions are disabled
during runtime modes.

Starting runtime also stops an active editor audio preview.

### Mouse capture in Play

Play requests mouse capture on entry.

Shift+F1 toggles capture when editor viewport navigation is not active.
Use this to regain pointer access when needed.

Stopping Play restores the capture state recorded before entering Play.

## Shortcut context

Keyboard actions can be suppressed when the UI captures keyboard input
or text entry.

Normal Scene and history shortcuts are suppressed during mouse capture.
Play and Simulate shortcuts are checked before that capture restriction,
but still respect UI keyboard/text capture.

When a shortcut appears inactive, finish text editing, release viewport
navigation, and check the current mode and focused panel.

## First editing exercise

1. Open a project and remain in Edit mode.
2. Select a visible Actor in the Scene panel.
3. Press F to focus it.
4. Change its position through Inspector or the translation gizmo.
5. Undo and redo the change.
6. Save with Ctrl+S.
7. Reload with Ctrl+O and confirm the saved transform.
8. Start Simulate with F6 and stop it with F6.
9. Confirm the editable Scene is retained.
10. Start and stop Play with F5.

Check Console messages if saving, loading, or runtime startup fails.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Actor cannot be selected in the Viewport | Use Scene selection; check navigation, gizmo, and drag state |
| Gizmo does not appear | Confirm Edit mode, an Actor selection, and an available Viewport |
| Inspector is disabled | Stop Play or Simulate |
| Shortcut does nothing | Check text entry, UI capture, mouse capture, panel focus, and mode |
| Runtime motion disappears after stopping | Runtime uses a disposable Scene copy |
| Scene does not reload as expected | Check the active path and the last successful save |
| Layout is difficult to recover | Use View → Reset Layout |

See [Editor internals](../Systems/Editor.md) for history and mutation
contracts and [Scene and Actors](../Systems/Scene.md) for persistence.