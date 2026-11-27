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
| Inspector | Actor properties, components, and selection summary |
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
or displayed debug icon. A viewport click selects a single Actor, and
clicking empty space clears selection.

Selection is suppressed during navigation capture, dragging assets,
and gizmo interaction.

An empty Actor has no rendered mesh, so select it through the hierarchy.

## Select multiple Actors

- Ctrl-click an Actor row in the Scene panel to add or remove it.
- Click an Actor, then Shift-click another to select the range in
  hierarchy order. The range includes Actor rows in expanded folders.

The Inspector shows the selection count and the primary Actor. Press
F to focus the primary Actor in the Viewport. Select a single Actor to
edit its properties, drag its row, duplicate it, delete it, or use the
transform gizmo.

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

## Test a project-authored HUD

A heads-up display (HUD) is the retained widget overlay drawn over the
Scene view. A Scene can select one UI document and one stylesheet. The
document describes layout and widgets. The stylesheet describes reusable
appearance values. Start in Edit mode with a saved Scene in a disposable
project.

### Create the source files

Save the following as `PlayHUD.quidoc`. This example uses canonical node
UUIDs and a style reference matching the stylesheet below. The Image node
starts with an empty texture reference. To show an image, import a texture
and paste its **Copy ID** value into `texture_id`.

```json
{
  "version": 1,
  "roots": [
    {
      "id": "ce4e18a5106b43d2-b201000000000001",
      "name": "Status",
      "type": "label",
      "bounds": { "position": [24, 24], "size": [200, 32] },
      "anchor": [0, 0], "pivot": [0, 0],
      "horizontal_alignment": "start", "vertical_alignment": "start",
      "enabled": true, "clip_to_bounds": false,
      "focusable": null, "hit_test": "none", "style_id": null,
      "properties": { "content": "Ready" },
      "children": []
    },
    {
      "id": "ce4e18a5106b43d2-b201000000000002",
      "name": "HUD image",
      "type": "image",
      "bounds": { "position": [24, 64], "size": [64, 64] },
      "anchor": [0, 0], "pivot": [0, 0],
      "horizontal_alignment": "start", "vertical_alignment": "start",
      "enabled": true, "clip_to_bounds": false,
      "focusable": null, "hit_test": "none", "style_id": null,
      "properties": { "texture_id": "" },
      "children": []
    },
    {
      "id": "ce4e18a5106b43d2-b201000000000003",
      "name": "Activate button",
      "type": "button",
      "bounds": { "position": [24, 144], "size": [160, 44] },
      "anchor": [0, 0], "pivot": [0, 0],
      "horizontal_alignment": "start", "vertical_alignment": "start",
      "enabled": true, "clip_to_bounds": false,
      "focusable": null, "hit_test": null,
      "style_id": "ce4e18a5106b43d2-c301000000000001",
      "properties": {},
      "children": [
        {
          "id": "ce4e18a5106b43d2-b201000000000004",
          "name": "Button caption",
          "type": "label",
          "bounds": { "position": [0, 0], "size": [0, 0] },
          "anchor": [0, 0], "pivot": [0, 0],
          "horizontal_alignment": "center", "vertical_alignment": "center",
          "enabled": true, "clip_to_bounds": false,
          "focusable": null, "hit_test": "none", "style_id": null,
          "properties": { "content": "Activate" },
          "children": []
        }
      ]
    }
  ]
}
```

Save this as `PlayHUD.quistyle`.

```json
{
  "version": 1,
  "styles": [
    {
      "id": "ce4e18a5106b43d2-c301000000000001",
      "name": "Activate",
      "type": "button",
      "properties": {
        "normal_color": [0.08, 0.32, 0.30, 1],
        "hovered_color": [0.10, 0.46, 0.43, 1],
        "pressed_color": [0.06, 0.23, 0.21, 1],
        "disabled_color": [0.20, 0.22, 0.22, 1],
        "focused_color": [0.13, 0.40, 0.48, 1],
        "padding": [10, 6, 10, 6]
      }
    }
  ]
}
```

### Select and play the HUD

1. Import both UI files through the [Asset Browser](Assets.md) and copy
   their registry UUIDs with **Copy ID**.
2. In the saved Scene JSON, add a top-level `hud` object with
   `document_asset_id` and `stylesheet_asset_id`. Use the copied registry
   UUIDs. See the [Scene file format](../Reference/SceneFiles.md) for the
   field layout.
3. Reload the Scene using **Load Scene** while in Edit. Enter Play and
   confirm that Ready and the styled button appear. If you set a registered
   texture UUID, confirm that its image appears too.
4. Use **Shift+F1** to release mouse capture before testing the button.
   Hover and press to check its styles. Stop to remove the HUD. Enter Play
   again to check that the HUD is rebuilt from disk.

The example files define the appearance and layout. To exercise label
activation with the shared TestDrive callback, close the Editor and set the
document record's `ID` in the disposable project's AssetRegistry.json to
`ce4e18a5106b43d2-a901000000000001`. Use a registry where that ID is free or
already belongs to this document. Put the same UUID in
`hud.document_asset_id` before reopening the project. The stylesheet keeps
its own registry UUID. The example node IDs match the ones used by
`Quiet::Samples::BindTestDriveHUD()` in `Samples/TestDriveHUD.h`.

In this fixture, click **Activate** to change Ready to Activated.
Stop and enter Play again to confirm the label returns to Ready. With
the viewport focused and capture released, Tab can focus the button, and
releasing Enter, keypad Enter, or Space can activate it.

A Scene with no `hud` object starts Play with an empty overlay. A
reported HUD mount failure leaves Play active and logs a warning in
Console. Check the Scene's asset UUIDs and types, the source file paths,
style references, and image texture availability. See the
[Cooking guide](Cooking.md) to run the same HUD from a package.

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
Open **Display** from the gear on the Viewport toolbar and toggle
**Navigation Guide** to show or hide the camera-control hints.

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
