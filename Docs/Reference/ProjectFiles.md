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

Example project record:

```json
{
    "Version": 1,
    "Assets": [
        {
            "ID": "Crate",
            "Type": "Mesh",
            "Path": "Assets/Crate.glb",
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
| Version | Integer | Defaults to 1; no compatibility rejection here |
| Assets | Array | Missing or non-array value produces no parsed project entries |

The root must be an object.

### Asset fields

| Field | JSON type | Missing-field default |
| --- | --- | --- |
| ID | String | Empty |
| Type | String | Unknown |
| Path | String | Empty |
| BuiltIn | Boolean | false |

Type strings are case-sensitive: Mesh, Texture, Audio, Script, and Unknown.

Unrecognized type text becomes Unknown.

A valid record requires only a nonempty ID and a type other than Unknown.
Path validity, source existence, and loaded-resource availability are
not established by this test.

Relative source paths are resolved by consuming systems against the
project root. Absolute paths can represent external sources.

### Loading

Non-object array entries and invalid records are skipped.

After reading, the loader restores built-in primitive records and sorts
records by ID. Built-in restoration replaces the first matching record
for each engine primitive ID.

General duplicate IDs are not rejected or deduplicated by this loader.
FindByID returns the first matching record.

An accepted registry can therefore contain skipped entries or duplicates.
Keep project IDs unique rather than relying on reader tolerance.

Parse errors return false; field-type extraction errors can propagate.
The output registry is replaced only after processing completes.

### Saving

The writer stores Version and valid records from the current registry.
Invalid records are omitted.

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