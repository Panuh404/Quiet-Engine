# Project Files <!--! {#quiet_format_project} -->

Quiet uses three distinct JSON documents:

| Document | Location | Responsibility |
| --- | --- | --- |
| Project descriptor | Project's .quiet file | Project metadata and directory settings |
| Asset registry | Project root/AssetRegistry.json | Registered asset identities and paths |
| User project registry | User profile/.quiet/ProjectRegistry.json | Projects root and recent-project history |

All three writers produce indented JSON.
Unknown fields are not retained when these writers reconstruct a document.

## Project descriptor

Implementation:

- Engine/include/Quiet/Project/ProjectDescriptor.h
- Engine/src/Project/ProjectDescriptor.cpp
- Quiet::Project::ProjectDescriptorIO

Example:

```json
{
    "Name": "FirstProject",
    "Version": 1,
    "CreatedAtUtc": "2026-10-12T12:00:00Z",
    "UpdatedAtUtc": "2026-10-12T12:00:00Z",
    "StartupScene": "DefaultScene.json",
    "ScenesDirectory": "Scenes",
    "AssetsDirectory": "Assets",
    "CacheDirectory": "Cache"
}
```

### Fields

| Field | JSON type | Missing-field default |
| --- | --- | --- |
| Name | String | QuietProject |
| Version | Integer | 1 |
| CreatedAtUtc | String | Current UTC timestamp |
| UpdatedAtUtc | String | CreatedAtUtc |
| StartupScene | String | DefaultScene.json |
| ScenesDirectory | String | Scenes |
| AssetsDirectory | String | Assets |
| CacheDirectory | String | Cache |

Empty Name, StartupScene, and directory strings are also replaced by
their defaults. Empty timestamps receive the timestamp fallbacks.

Timestamp generation uses UTC text in the form YYYY-MM-DDTHH:MM:SSZ.
The loader does not validate nonempty timestamp strings against that
format.

### Paths

The descriptor's parent directory becomes the project root.

ProjectContext combines that root with ScenesDirectory,
AssetsDirectory, and CacheDirectory. StartupScene is combined with
the Scenes directory.

The descriptor reader does not enforce project-relative containment,
validate every directory, or check that the startup Scene exists.

### Loading and saving

Loading requires a JSON object. File-open failure, JSON parse failure,
or a non-object root returns false.

Field extraction occurs outside the parse exception handler.
Wrong field types can therefore propagate exceptions.

Version is read as metadata; this loader does not reject versions other
than 1 or perform version-specific migration.

Loading replaces the output descriptor only after field extraction and
fallback handling complete.

Saving fills an empty CreatedAtUtc and always updates UpdatedAtUtc.
It writes all listed fields. The descriptor is passed by value, so these
timestamp changes do not modify the caller's object.

## Asset registry

Implementation:

- Engine/include/Quiet/Project/AssetRecord.h
- Engine/include/Quiet/Project/AssetRegistry.h
- Engine/src/Project/AssetRegistry.cpp
- Quiet::Project::AssetRegistryIO

Example project registry:

```json
{
    "Version": 2,
    "Assets": [
        {
            "ID": "6547bbc2e20c9127-bbd4ad89f5970ec1",
            "Name": "Crate",
            "Type": "Mesh",
            "Path": "Assets/Crate.glb",
            "BuiltIn": false
        },
        {
            "ID": "ce4e18a5106b43d2-a901000000000001",
            "Name": "Play HUD",
            "Type": "UIDocument",
            "Path": "Assets/PlayHUD.quidoc",
            "BuiltIn": false
        },
        {
            "ID": "ce4e18a5106b43d2-a901000000000002",
            "Name": "Play HUD Styles",
            "Type": "UIStyleSheet",
            "Path": "Assets/PlayHUD.quistyle",
            "BuiltIn": false
        }
    ]
}
```

The example omits engine-provided records. Loading restores the built-in
mesh records supplied by Quiet.

### Root fields

| Field | JSON type | Reader behavior |
| --- | --- | --- |
| Version | Integer | Required. Must be 2 |
| Assets | Array | Required. Each entry must be a valid asset object |

The root must be an object.

### Asset fields

| Field | JSON type | Reader behavior |
| --- | --- | --- |
| ID | Canonical UUID string | Required, nonzero, and unique in the array |
| Name | String | Required and nonempty |
| Type | String | Required and case-sensitive |
| Path | String | Defaults to empty |
| BuiltIn | Boolean | Defaults to false |
| MeshTextureBindings | Object | Optional for external meshes. Maps source keys to canonical texture UUIDs |

Accepted record types are Mesh, Texture, Audio, Script, UIDocument,
and UIStyleSheet. UI source paths conventionally use `.quidoc` and
`.quistyle`, respectively. The corresponding UUIDs can be selected in a
[Scene's HUD fields](SceneFiles.md).

A valid record establishes identity, display name, and type. Source
existence and loaded-resource availability are checked by the consumer.

Relative source paths are resolved by consuming systems against the
project root. Absolute paths can represent external sources.

### Loading

The loader returns false for malformed JSON, wrong field types, unknown
fields, duplicate UUIDs, or invalid records. Reserved UUID rules limit
built-in records to Quiet's supplied primitive meshes. On success, the
loader restores the primitive records, sorts by UUID, and publishes the
result. A reported failure preserves the caller's previous registry.

### Saving

The writer validates the current version and every record before saving.
An invalid record or duplicate UUID makes it return false.

It writes the supplied record order; normal registry operations maintain
sorted order, but saving alone does not sort an arbitrary registry.

Registry persistence does not copy files or load runtime resources.

## User project registry

Implementation:

- Engine/include/Quiet/Project/ProjectRegistry.h
- Engine/src/Project/ProjectRegistry.cpp
- Quiet::Project::ProjectRegistryIO

Windows example:

```json
{
    "ProjectsRoot": "C:\\Users\\Example\\Documents\\QuietProjects",
    "LastOpenedProjectFile": "C:\\Users\\Example\\Documents\\QuietProjects\\FirstProject\\FirstProject.quiet",
    "RecentProjectFiles": [
        "C:\\Users\\Example\\Documents\\QuietProjects\\FirstProject\\FirstProject.quiet"
    ]
}
```

### Fields

| Field | JSON type | Reader behavior |
| --- | --- | --- |
| ProjectsRoot | String | Missing or empty uses the default projects root |
| LastOpenedProjectFile | String | Missing or empty remains unset |
| RecentProjectFiles | Array of strings | Missing/non-array remains empty; non-string entries are skipped |

This document has no Version field.

The default paths are constructed beneath the detected home directory:

- Documents/QuietProjects
- .quiet/ProjectRegistry.json

Windows home detection tries USERPROFILE, then HOMEDRIVE with HOMEPATH.
Other platforms use HOME. Detection falls back to the current directory.

These paths do not query Windows Documents-folder redirection.

### History behavior

TouchRecentProject() sets LastOpenedProjectFile, removes equal existing
entries, inserts the path at the front, and trims to the requested limit.
Its default limit is twelve.

Loading itself does not enforce that limit, deduplicate history, or
verify that stored projects still exist.

Project discovery separately scans ProjectsRoot recursively for regular
files with the exact .quiet extension.

### Failure behavior

Loading requires an object root. Open and parse failures return false;
field extraction errors can propagate.

Saving writes the configured root, last-opened path, and recent list.

## Write boundaries

These writers write directly to their destinations rather than using
atomic temporary-file replacement.

They report stream status after writing but do not provide a checked
close guarantee or rollback of earlier filesystem changes.

Prefer Editor and Launcher operations for normal changes.
Use [Projects](../Guides/Projects.md) and [Assets](../Guides/Assets.md)
for those workflows.
