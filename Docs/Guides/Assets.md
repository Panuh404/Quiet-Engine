# Assets

This guide covers importing, inspecting, assigning, organizing, and
removing project assets through the Editor's Assets panel.

## Prerequisites

Open an editable project and remain in Edit mode.
See [Projects](Projects.md) and [Editor](Editor.md).

Asset file and registry operations are separate from Scene history.
Do not expect Ctrl+Z to undo project filesystem changes.

## Browse assets

Use the Assets panel to browse project folders or built-in engine assets.

The toolbar provides:

- Import.
- Search.
- Asset-type filtering.
- Diagnostics filtering.
- List or Browser view.
- Browser thumbnail sizing.

Diagnostics distinguish missing paths and external sources.
A registered asset can exist without a successfully loaded resource.

Built-in assets can be used in Scenes but are protected from project
file operations.

## Supported import types

The import interface detects type from the extension, case-insensitively:

| Type | Extensions |
| --- | --- |
| Mesh | .obj, .fbx, .gltf, .glb, .dae |
| Texture | .png, .jpg, .jpeg, .tga, .bmp |
| Audio | .wav, .flac, .mp3 |
| Script | .lua |

Extension acceptance does not guarantee successful decoding or loading.

## Import an asset

1. Select the destination project folder in Assets.
2. Select Import, or use Import Asset... from the folder context menu.
3. Select Browse and choose one source file, or enter its path.
4. Enter an Asset ID, or leave it empty to derive one from the filename.
5. Review the destination and validation messages.
6. Select Import.

Relative source paths are resolved against the project root.

The import copies the selected file into the destination folder using
its original filename. If it is already that destination file, it can
be registered without copying.

Import does not overwrite a different file with the same filename.
It also rejects an existing asset ID or an already registered
destination file.

Expected result: the registry gains the asset and the Editor queues
asset loading. Check Console and asset details for loading failures.

### Files with external dependencies

This import operation copies the selected file, not its complete
dependency tree.

Meshes can reference additional files, such as glTF buffers or material
textures. Preserve those files in the required relative locations and
verify loading. Texture assignments may also require separately
registered texture assets.

## Inspect and reimport

Right-click an asset and select Inspect to view its details.

Available context actions include Copy ID, path copying, and Reveal in
File Manager. Script assets also provide Open Script through the
system's associated application.

For an existing mesh, texture, or audio asset whose source changed,
select Reimport.

Reimport uses the registered source path and retains the asset ID.
It is unavailable for built-ins, missing sources, and runtime modes.

Script source assets do not use this Reimport action.
See [Scripting internals](../Systems/Scripting.md) for runtime reload
and failed-load behavior.

## Assign assets to Actors

Use the matching Inspector asset picker:

| Asset type | Typical destination |
| --- | --- |
| Mesh | Mesh component |
| Texture | Texture or material texture field |
| Audio | AudioSource component |
| Script | Script component |
| Typed exposed reference | Matching script property field |

Assignments use asset IDs rather than copying resources into the Actor.

In Edit mode, dragging a registered, loaded mesh onto the Viewport
creates a mesh Actor. Non-mesh drops are rejected by this workflow.

Save the Scene after changing assignments.

## Preview audio

Inspect an audio asset whose metadata is loaded, then select Preview.

Restart Preview begins a replacement preview.
Stop releases the preview sound.

The subsystem owns one preview at a time.
Preview is nonspatial and uses Master volume.

Starting Play or Simulate stops the Editor preview.
An unavailable preview can indicate missing metadata or a playback
initialization failure; check Console.

## Organize project assets

Use Create Folder from a project-folder context menu to prepare
destinations.

| Shortcut in the Assets panel | Operation |
| --- | --- |
| Ctrl+C | Copy selected assets |
| Ctrl+X | Cut selected assets |
| Ctrl+V | Paste copies or move cut assets into the current folder |
| Ctrl+D | Duplicate selection |
| F2 | Rename one selected asset's source file |
| Delete | Request deletion from the project |

These operations require Edit mode and eligible project assets.
Keyboard handling also depends on panel focus and text-entry state.

Copying and duplication create new assets with distinct IDs.
Moving and source-file renaming retain existing asset identity.

The source-file rename operation is not an Asset ID rename.

Use the Editor's operations when reorganizing registered files so registry
paths remain coordinated. Check operation feedback when a batch fails;
do not assume every selected item changed successfully.

## Remove registration or delete files

These are different operations:

| Action | Effect |
| --- | --- |
| Remove from Registry... | Removes registration while retaining the source file |
| Delete from Project... | Removes registration and permanently deletes the eligible source file |

Both use confirmation dialogs and check references in the current
in-memory Scene. Referenced selections can prevent completion.

The reference check does not scan every saved Scene in the project.
An unreferenced result for the current Scene is not proof that other
Scenes do not use the asset.

Permanent deletion requires an eligible physical source inside the
project Assets directory and an explicit acknowledgment.

Built-in and external sources are protected from this deletion path.
Folder operations also have their own eligibility and confirmation
checks.

## Verification exercise

Use a disposable test asset:

1. Import it into a project folder.
2. Inspect it and confirm successful loading.
3. Assign it to a suitable Actor component.
4. Save and reload the Scene to confirm the assignment persists.
5. Move or rename the source through Assets.
6. Confirm the same asset ID still resolves.
7. Remove its Scene references before testing registry removal or deletion.
8. Review Console and operation feedback.

Check other saved Scenes before permanently deleting shared assets.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Import is disabled | Source path, supported extension, destination availability, and filename conflict |
| Asset ID already exists | Use the existing asset's Reimport action or choose a distinct ID |
| Asset is registered but not loaded | Inspect details and Console; check source validity and mesh dependencies |
| Mesh drop is rejected | Confirm Edit mode, Mesh type, registration, source availability, and loaded resource |
| Rename or move is unavailable | Check built-in/external status, selection, current source, and mode |
| Removal is blocked | Clear current Scene references and review the confirmation dialog |
| Audio preview fails | Check loaded metadata, source availability, and playback diagnostics |

See [Assets internals](../Systems/Assets.md) for registry and loading
contracts and [Audio](../Systems/Audio.md) for preview behavior.