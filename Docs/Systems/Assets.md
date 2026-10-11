# Assets

Quiet separates asset records, source files, loading operations, and
loaded resources.

The project registry describes assets by ID, type, path, and built-in
status. Loaders use those records to populate resource libraries.

This chapter covers asset identity, registry persistence, built-in records,
source paths, ownership, editable importing, incremental loading, resource
replacement, and runtime package loading.

See [Application](Application.md) for loader construction and startup,
and [Scene and Actors](Scene.md) for Scene resource references.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Asset type classification | `Engine/include/Quiet/Project/AssetType.h` |
| Asset record | `Engine/include/Quiet/Project/AssetRecord.h` |
| Registry declarations | `Engine/include/Quiet/Project/AssetRegistry.h` |
| Registry operations and persistence | `Engine/src/Project/AssetRegistry.cpp` |
| Project paths and registry ownership | `Engine/include/Quiet/Project/ProjectContext.h` |
| Editable project loader declaration | `Engine/include/Quiet/Project/ProjectAssetLoader.h` |
| Editable project loading | `Engine/src/Project/ProjectAssetLoader.cpp` |
| Runtime manifest declarations and validation | `Engine/include/Quiet/Project/RuntimeManifest.h`, `Engine/src/Project/RuntimeManifest.cpp` |
| Runtime resource loader | `Engine/include/Quiet/Project/RuntimeAssetLoader.h`, `Engine/src/Project/RuntimeAssetLoader.cpp` |
| Texture registration | `Engine/src/Project/ProjectContext.cpp` |
| Reload request dispatch and Scene mesh relinking | `Engine/src/Core/Application.cpp` |
| Mesh library | `Engine/src/Renderer/Vulkan/Mesh/MeshLibrary.h` |
| Texture library | `Engine/src/Renderer/Vulkan/Texture/TextureLibrary.h` |
| Audio library | `Engine/src/Audio/AudioLibrary.h` |

## Asset identity

AssetRecord::ID is a string used for registry lookup and resource loading.

It is distinct from Actor UUIDs. AssetRecord does not require its ID
to follow a UUID format.

AssetType classifies records as Mesh, Texture, Audio, Script, or Unknown.

Registry lookup uses the ID alone, not an ID-and-type pair.
AddOrUpdate() can replace a matching record's type and other fields.

Asset IDs and source paths serve different purposes: the ID identifies
the record, while Path tells a loader where to find source data.

## Asset records

Quiet::Project::AssetRecord contains:

| Field | Meaning |
| --- | --- |
| ID | String identity used for lookup |
| Type | Asset classification |
| Path | Source path interpreted by the relevant loading operation |
| BuiltIn | Marks an engine-provided asset |

IsValid() checks only that ID is nonempty and Type differs from Unknown.

It does not validate:

- Whether Path is populated or exists.
- Whether the file can be imported.
- Whether a resource has been loaded.
- Whether a GPU resource is ready.
- Whether BuiltIn is consistent with the other fields.

A valid record is therefore metadata accepted by this check, not proof
of a usable asset.

## Registry operations

AssetRegistry stores a Version value, initially 1, and a vector of records.

ProjectContext stores the project's registry and exposes mutable and
const access to it.

### Adding or updating

AssetRegistryIO::AddOrUpdate() rejects records whose IsValid() is false.

Otherwise, it replaces the first record with the same ID, or appends
a new record, then sorts records by ID.

True means that the record was accepted. It does not mean that its contents
differed from the previous record, that a file was saved, or that loading
succeeded.

### Removing

RemoveByID() removes every matching ID and sorts the remaining records.

It returns false when no matching record exists.

This operation changes registry metadata only. It does not delete source
files, unload resources, or repair Scene references.

The method itself does not prevent removal of a built-in record.

### Looking up records

FindByID() returns a pointer to the first matching record, or nullptr.
It does not independently check IsValid().

GetAssetsByType() returns pointers to matching valid records.

GetAssetIDsByType() returns sorted ID copies for matching valid records.

### Pointer lifetime

Record pointers refer to elements of AssetRegistry::Records.

Do not retain them across registry mutation. Appending, erasing, sorting,
or replacing records can invalidate pointers or change which record occupies
a particular storage location.

Retain an ID and perform lookup again when a later operation needs the record.

## Source paths

External mesh and texture loading requires a nonempty source path when
loading proceeds. An existing library entry can bypass source validation
when forceReimport is false.

External audio loading checks its source path before calling AudioLibrary,
even when that library already contains the asset.

For these loaders:

- Absolute paths are used directly.
- Relative paths are resolved against ProjectContext::GetRootPath().
- The resulting path is lexically normalized.

Relative source paths are resolved against the project root, not
automatically against its Assets directory.

Lexical normalization is not an existence check or a guarantee that the
path remains inside the project.

Built-in mesh loading uses primitive generation rather than the external
source-file path.

Script loading and path handling are covered separately by the scripting
system; do not assume that all asset types use the same loader.

## Loaded resources and ownership

| Object | Ownership |
| --- | --- |
| ProjectContext | Stores project asset registry metadata |
| VulkanRenderer | Owns MeshLibrary and TextureLibrary |
| AudioSubsystem | Owns AudioLibrary |
| MeshLibrary | Stores shared pointers to StaticMesh resources and mesh statistics |
| TextureLibrary | Stores shared pointers to Texture2D resources and texture statistics |
| AudioLibrary | Stores AudioAsset values |
| ProjectAssetLoader | Borrows project context, libraries, and renderer resources |

ProjectAssetLoader coordinates loading. It does not own the destination
libraries or transfer their ownership into itself.

Renderer resources are stored by the loader as pointers and can be rebound
through BindRendererResources().

These borrowed dependencies must remain valid while loading uses them.

### Registration versus resource availability

The project registry and resource libraries are separate stores.
A registry record can exist while loading fails.

TextureLibrary also separates statistics from GPU texture objects.
Has() checks its statistics map, and GetCount() counts statistics entries.

Consequently, TextureLibrary::Has() returning true does not establish
that TryGet() can return a Texture2D.

Use the query that matches the requirement: metadata presence, resource
availability, and descriptor availability are different questions.

### Shared resource lifetime

Mesh and texture libraries store shared pointers.
Other holders can keep a resource alive beyond the library's own reference.

This does not establish that every consumer automatically switches to
a replacement resource. Replacement and Scene relinking contracts will
be documented in the loading increment.

## Registry persistence

AssetRegistryIO reads and writes registry JSON independently of resource
loading.

ProjectContext's registry file path is the project root's AssetRegistry.json.

### Loading

LoadFromFile() opens and parses the file, requires an object root, and
builds a temporary registry.

It reads Version and the Assets array when present, then restores built-in
records, sorts by ID, and assigns the completed registry to outRegistry.

| Input condition | Behavior |
| --- | --- |
| File cannot be opened | Returns false |
| JSON parsing throws | Returns false |
| Root is not an object | Returns false |
| Assets is absent or not an array | Continues without external records |
| An array entry is not an object | Skips that entry |
| An entry has an empty ID or Unknown type | Skips that entry |
| A field has an incompatible JSON type | Can throw during field extraction |

Only the JSON parsing operation is inside the catch block.
Field extraction and subsequent operations can propagate exceptions.

Because assignment to outRegistry occurs at the end, earlier failures
leave the caller's registry unchanged.

True means that loading completed under these rules. It does not prove
that every supplied entry was accepted or that referenced files exist.

Version is read as metadata; this function does not reject unsupported
version numbers or perform a version migration.

### Duplicate IDs

Loading appends accepted external records directly. It does not generally
deduplicate their IDs.

FindByID() returns the first match, and AddOrUpdate() replaces the first
match. Neither establishes uniqueness for an already duplicated registry.

RemoveByID() removes every match.

After sorting, do not depend on which duplicate record will be returned
when duplicate IDs are present.

### Saving

SaveToFile() attempts to create the parent directory, opens the destination,
and writes Version and valid records as formatted JSON.

Invalid records are omitted.

It returns false when opening fails and otherwise returns output.good()
after writing.

Directory creation uses an error_code, but its result is not independently
returned; opening the destination determines whether writing can proceed.

Saving does not use a temporary-file replacement or check stream status
after an explicit close. Its result is not an atomic-save or durability
guarantee.

Serialization and other uncaught exceptions can propagate.

## Built-in records

MakeDefault() creates a registry and calls EnsureBuiltInAssets().

EnsureBuiltInAssets() obtains primitive IDs from MeshPrimitives and upserts
each as:

- Type Mesh.
- Empty source path.
- BuiltIn true.

It sorts records afterward.

An existing first match with a primitive ID is overwritten with this
built-in record. Duplicate matches are not all removed.

LoadFromFile() also calls EnsureBuiltInAssets(), so loading restores the
primitive records even when the file omits them.

The general registry mutation methods do not reserve these IDs or prevent
built-in removal. Restoration occurs when EnsureBuiltInAssets() runs.

## Editable-project loading

ProjectAssetLoader's batch operations use the current project registry
to populate the existing libraries.

### Batch selection

| Method | Records processed |
| --- | --- |
| ReloadMeshAssets() | Valid Mesh records, including built-in primitives |
| ReloadBuiltInMeshAssets() | Valid Mesh records marked BuiltIn |
| ReloadTextureAssets() | Valid external Texture records |
| ReloadAudioAssets() | Valid external Audio records |
| ReloadRuntimeSupportAssets() | Delegates to built-in mesh loading |
| ReloadAssets() | Meshes, then textures, then audio |

ReloadAssets() does not load Script records through these resource libraries.

Mesh, texture, and audio batch loaders copy their selected records before
processing. This avoids retaining registry element pointers during that
batch's loading operations.

### Results and partial success

A reported per-asset false result sets the batch's aggregate result to false.
The loop continues processing the remaining selected records.

ReloadAssets() calls all three batches before combining their results.
A mesh batch's false result does not by itself skip textures or audio.

These guarantees concern false results. An uncaught exception can interrupt
the sequence.

| Result | Meaning |
| --- | --- |
| True | No processed record reported failure |
| False | At least one processed record reported failure |
| Exception | Execution stopped without an aggregate result |

An empty batch can return true. Built-in texture and audio records are
skipped by their batch loaders.

False does not undo earlier successful loads. These operations do not
provide a transaction across libraries.

### Existing resources

With forceReimport false, external mesh loading returns true for an existing
MeshLibrary entry after checking renderer bindings.

External texture loading similarly uses TextureLibrary::Has().
Because Has() checks statistics, this shortcut does not independently
establish that a GPU texture object exists.

External audio loading validates a nonempty path and file existence before
calling AudioLibrary. AudioLibrary can then retain an existing entry when
replacement is not requested.

forceReimport requests replacement behavior for external assets. It does
not automatically mean every resource is rebuilt; built-in mesh loading
has its own existing-resource shortcut.

### Loading one ID

ReloadAssetByID() looks up a registry record and dispatches by type.

| Record | Behavior |
| --- | --- |
| Missing ID | Returns false |
| Built-in Mesh | Uses primitive loading |
| External Mesh, Texture, or Audio | Uses the corresponding external loader |
| Built-in Texture or Audio | Returns false |
| Script | Returns false; treated as source-only |
| Unknown type | Returns false |

Its default forceReimport value is true, unlike the batch methods whose
default is false.

Registration, load success, and resource replacement remain separate
contracts.

## External importing

Editable-project loading imports source data before creating or replacing
the corresponding library resource.

### Mesh loading

LoadExternalMesh() performs these operations when loading is required:

1. Checks renderer bindings and resolves the source path.
2. Requires the source path to exist.
3. Calls MeshImporter::LoadFromFile().
4. Rejects an unsuccessful import result.
5. Registers supported material textures.
6. Builds mesh statistics, submeshes, and material slots.
7. Creates or replaces the StaticMesh resource.

Import warnings are recorded and logged. They do not automatically make
a successful mesh load return false.

Material texture registration occurs before GPU mesh creation.
A later resource failure does not roll back extracted files or registered
texture records.

### Texture loading

LoadExternalTexture() resolves the source and calls
TextureImporter::LoadPixelsFromFile().

It requires both successful metadata and valid pixel data before creating
or replacing a Texture2D.

The resulting statistics include decoded pixel size and importer warnings.

### Exception boundaries

External mesh and texture loading catch std::exception around their
resource creation or replacement calls and return false.

These catch blocks do not enclose the complete loading operation.
Filesystem checks, import calls, and mesh material registration can
propagate exceptions.

A false result is therefore a reported failure, not a promise that the
operation had no side effects.

## Imported material textures

Mesh loading examines imported materials with base-color texture paths.

It derives a texture asset ID from the mesh ID, material name, and material
index.

### External texture references

Relative material texture paths are resolved against the mesh source
file's parent directory.

This differs from a registry record's relative Path, which external asset
loading resolves against the project root.

The registration helper requires the referenced file to exist and accepts
these extensions, case-insensitively:

- .png
- .jpg
- .jpeg
- .tga
- .bmp

Successful registration assigns the generated asset ID to the imported
material's BaseColorTextureAssetID.

Missing files, unsupported extensions, and registration failures add
warnings and allow processing of the remaining materials.

### Embedded textures

Embedded references are resolved against the import result's embedded
texture records.

Valid texture bytes are written beneath:

Assets directory / Textures / mesh asset ID / generated texture filename

The filename combines the generated texture ID and embedded extension.

The extracted file is then registered as an external texture asset.
Registration does not itself decode or upload the texture.

### Extraction comparison

WriteBinaryFileIfDifferent() skips writing when an existing file has the
same byte count as the incoming data.

Despite its name, it does not compare file contents.
Changed bytes with an unchanged length can therefore retain the previous
extracted file.

This is the current implementation contract, not a content-integrity check.

### Loading newly registered textures

ReloadAssets() loads meshes before selecting texture records.
Textures registered by mesh import can therefore participate in that
subsequent texture batch.

Incremental loading likewise selects texture records when it prepares
the texture stage, after the mesh stage.

Reloading one mesh through ReloadAssetByID() does not independently run
the texture batch afterward.

## Incremental loading

BeginIncrementalReload() resets progress, stores forceReimport, clears
AudioLibrary, and prepares the mesh stage.

It does not clear MeshLibrary or TextureLibrary.

Starting a new incremental sequence replaces the loader's previous
progress and pending record list. It does not roll back resources already
loaded by an earlier sequence.

### Stages

The sequence progresses through Meshes, Textures, Audio, and Complete.

| Stage | Selected records |
| --- | --- |
| Meshes | Valid mesh records, including built-ins |
| Textures | Valid external texture records |
| Audio | Valid external audio records |
| Complete | No further loading work |

Each stage copies records from the registry when that stage is prepared.
The whole sequence is not one immutable registry snapshot.

Empty stages advance immediately during preparation. An empty sequence
can reach Complete during BeginIncrementalReload().

### Advancing

AdvanceIncrementalReload() does nothing in Idle or Complete.

Otherwise, it processes at most one asset record, or advances between
stages when no records remain.

A false load result sets HadFailures. The record index still advances,
allowing later records to be attempted.

This work executes synchronously in the caller. Incremental scheduling
does not guarantee asynchronous execution or a bounded frame duration.

### Progress interpretation

| Field | Meaning |
| --- | --- |
| Stage | Current loading stage |
| Completed | Records attempted in the current stage |
| Total | Records selected for the current stage |
| CurrentAssetID | Next pending record ID after an advance |
| HadFailures | Whether a processed loader reported false during this sequence |

Completed and Total reset when a new stage is prepared.
They are not cumulative counts for the entire project.

Failed records still count toward Completed.

IsIncrementalReloadComplete() checks only whether Stage is Complete.
A completed sequence can have HadFailures set.

Exceptions are not converted into HadFailures by AdvanceIncrementalReload().
They can interrupt the sequence before the record index advances.

## Resource replacement

MeshLibrary::ReplaceStaticMesh() and TextureLibrary::ReplaceTexture2D()
create new resource objects and assign them under the existing library key.

They do not modify every external shared pointer to the previous object.

A holder of an old shared pointer can continue referencing the old resource
until it explicitly adopts the replacement or releases its reference.

### Scene mesh relinking

Application's asset reload and reimport request paths call
RelinkSceneStaticMeshes() for the editable Scene and any runtime Scene.

That helper resolves each StaticMeshComponent's MeshResourceID against
MeshLibrary and assigns the current shared pointer when found.

When lookup fails, it logs a warning and leaves the component's existing
pointer unchanged.

Calling ProjectAssetLoader directly does not automatically perform this
Application-level relinking.

### Replacement failures

Resource construction can fail before the new object is assigned.
Other steps, such as statistics updates or earlier import side effects,
remain separate operations.

Do not treat reimport as a transaction across the registry, source files,
resource libraries, and Scene references.

## Runtime package loading

Quiet::Project::RuntimeAssetLoader loads manifest records into existing
mesh, texture, and audio libraries.

It borrows those libraries and renderer dependencies. Destroying the
loader does not destroy the loaded resources.

Application handles package selection, manifest loading, project-context
initialization, startup Scene loading, and runtime entry.

See [Application](Application.md) for that startup sequence.

### Runtime records

RuntimeAssetRecord contains ID, Type, and Path.

Unlike AssetRecord::IsValid(), its validity check also requires a nonempty
Path. It still does not establish file existence or resource availability.

RuntimeAssetLoader receives an existing manifest. It does not read
RuntimeManifest.json or independently validate the entire manifest.

### Paths and validation boundaries

Each per-asset loader constructs:

(packageRoot / record.Path).lexically_normal()

It then requires the result to be a regular file.

Normal manifest IO validates package-relative paths and rejects duplicate
asset IDs. The resource loader itself does not repeat those whole-manifest
checks.

Callers supplying a manifest directly must preserve that validation
boundary. Lexical normalization alone does not enforce package containment.

### Resource paths

| Asset type | Runtime loading |
| --- | --- |
| Mesh | QMeshReader, then StaticMesh creation or replacement |
| Texture | QTextureReader, then Texture2D creation or replacement |
| Audio | AudioLibrary::LoadFromFile() with replacement enabled |
| Script | Not processed by these three resource-loading methods |

Runtime mesh and texture loading uses cooked readers rather than the
editable source importers.

Runtime audio loading passes the packaged file to AudioLibrary. The log's
description of a cooked audio asset does not imply a QAudio reader or
QAudio binary format.

Script execution belongs to the scripting system.

### Cooked mesh metadata

Runtime mesh loading reconstructs submesh ranges and material-slot indices.

QMesh v1 does not store imported submesh names. Runtime assigns names such
as Submesh_0 and Submesh_1.

The loader supplies an empty imported-material-slot vector to MeshLibrary.
It does not reconstruct source importer material metadata or register
material textures during this path.

### Existing resources

Runtime mesh and texture loading reads the packaged data before creating
or replacing the resource under its asset ID.

Unlike editable loading with forceReimport false, these paths do not return
early merely because the library already contains the ID.

Runtime audio loading also requests replacement of an existing entry.

External shared pointers still follow the replacement contracts described
earlier in this chapter.

### Batch results

Each public loading method selects manifest records of its corresponding
type and attempts them in manifest order.

A per-asset false result marks the aggregate result false, but processing
continues with the remaining records of that type.

An empty batch returns true.

These methods do not provide rollback across records. Successfully loaded
resources can remain after another record fails.

Mesh and texture resource creation catch std::exception and report false.
Other uncaught exceptions can interrupt the batch.

### Application failure handling

Application invokes runtime mesh, texture, and audio batches in sequence.

A false batch result causes Sandbox startup to throw before invoking the
next batch. This differs from editable ReloadAssets(), which calls all
three batches before combining their results.

Startup Scene loading occurs after the required asset batches succeed.
A Scene load failure does not undo those completed asset loads.

## Worked example: registry acceptance

This example uses an in-memory registry. It does not require an initialized
renderer or perform asset loading.

    Quiet::Project::AssetRegistry registry{};

    Quiet::Project::AssetRecord record{};
    record.ID = "ExampleMesh";
    record.Type = Quiet::Project::AssetType::Mesh;
    record.Path = "Assets/Meshes/Example.glb";

    const bool accepted = Quiet::Project::AssetRegistryIO::AddOrUpdate(registry, record);

    const auto* found = Quiet::Project::AssetRegistryIO::FindByID(registry, "ExampleMesh");

accepted can be true even when the source file does not exist.
found points into registry storage; do not retain it across registry mutation.

Adding the record does not save AssetRegistry.json or create a StaticMesh.

Use the appropriate subsequent operation for persistence or loading, and
evaluate that operation's result separately.

## Extension guidance

Adding an asset type requires more than extending AssetType.

Review registry serialization, project registration, batch selection,
incremental stages, resource ownership, runtime loading, cooking, and
Scene reference handling where applicable.

When adding reload behavior, distinguish:

- Record acceptance.
- Source validation and importing.
- Resource creation or replacement.
- Consumer relinking.
- Registry persistence.

A success result at one stage does not establish success at every stage.

Binary layouts and exact JSON schemas belong in the planned file-format
reference. Package construction belongs in the Cooking chapter.