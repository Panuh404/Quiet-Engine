# Cooking <!--! {#quiet_system_cooking} -->

Quiet_Cooker converts and packages registered project assets for
standalone runtime loading.

Its main function owns the project inputs, cache state, and collected
runtime asset records. Asset stages borrow those objects through
CookContext.

This chapter covers orchestration, ownership, cache persistence,
conversion and native packaging, writer contracts, validation,
finalization, stale-output cleanup, and runtime package consumption.

See [Assets](Assets.md) for asset identity and loading responsibilities
and [Application](Application.md) for Sandbox startup.

## Implementation map

| Responsibility | Source |
| --- | --- |
| CLI validation and stage orchestration | `Cooker/src/main.cpp` |
| Borrowed stage context | `Cooker/src/Core/CookContext.h` |
| File hashes and persistent cook cache | `Cooker/src/Core/CookCache.h` and `.cpp` |
| Mesh cooking | `Cooker/src/Mesh/MeshCooker.h` and `.cpp` |
| Quantized mesh encoding and QAsset writing | `Cooker/src/Mesh/QMeshWriter.h` and `.cpp` |
| Texture cooking | `Cooker/src/Texture/TextureCooker.h` and `.cpp` |
| sRGB mip generation | `Cooker/src/Texture/QTextureMipGenerator.h` and `.cpp` |
| BC7 block compression | `Cooker/src/Texture/QTextureCompressor.h` and `.cpp` |
| QTexture and embedded KTX2 writing | `Cooker/src/Texture/QTextureWriter.h` and `.cpp` |
| Audio packaging | `Cooker/src/Audio/AudioCooker.h` and `.cpp` |
| Script packaging | `Cooker/src/Script/ScriptCooker.h` and `.cpp` |
| Output directories and stale-asset pruning | `Cooker/src/Package/PackageLayout.h` and `.cpp` |
| Startup scene and package finalization | `Cooker/src/Package/PackageBuilder.h` and `.cpp` |
| Runtime manifest representation | `Engine/include/Quiet/Project/RuntimeManifest.h` |
| Manifest validation and persistence | `Engine/src/Project/RuntimeManifest.cpp` |
| Runtime package context initialization | `Engine/src/Project/ProjectContext.cpp` |
| Sandbox package startup | `Engine/src/Core/Application.cpp` |
| Runtime package asset loading | `Engine/src/Project/RuntimeAssetLoader.cpp` |

## Command-line inputs

The executable requires exactly two arguments:

```text
Quiet_Cooker <project.quiet> <output-directory>
```

An incorrect argument count prints usage and returns exit code 1.

After argument-count validation, the Cooker validates the
Compressonator BC7 codec before loading the project.

Project and output paths are resolved to absolute, lexically normalized
paths. The project argument must identify an available regular file
that ProjectDescriptorIO can load.

The project root is the project descriptor's parent directory.

The Cooker loads AssetRegistry.json from that root and resolves the
startup scene using the descriptor's ScenesDirectory and StartupScene.
The startup scene must be an available regular file.

## Ownership and shared context

main() owns:

- Project and output root paths.
- ProjectDescriptor and AssetRegistry.
- Previous and next CookCache objects.
- The collected RuntimeAssetRecord vector.

CookContext contains references to these stage inputs and outputs.
It does not own them or extend their lifetime.

| Context member | Stage access |
| --- | --- |
| ProjectRoot | Read-only project root |
| OutputRoot | Read-only output root |
| AssetRegistry | Read-only registered assets |
| PreviousCache | Read-only cache from the preceding cook |
| NextCache | Mutable cache being assembled |
| RuntimeAssets | Mutable runtime asset records being collected |

The referenced objects remain alive throughout the synchronous pipeline.

## Pipeline order

After validating project inputs, main():

1. Prepares the output directories.
2. Loads the previous cook cache.
3. Creates an empty next cache and runtime asset list.
4. Cooks meshes.
5. Cooks textures.
6. Packages audio.
7. Packages scripts.
8. Finalizes the runtime package.

A stage returning false stops later stages and returns exit code 1.

Preparing output and writing assets happen before finalization.
A later failure does not roll back earlier filesystem changes.

The CLI's displayed asset counts exclude built-in records and count
registered Mesh, Texture, Audio, and Script assets. These counts do not
establish successful conversion or availability.

## Output layout

PrepareOutputLayout() creates these directories as needed:

| Directory | Responsibility |
| --- | --- |
| Scenes | Packaged startup scene |
| Content/Meshes | Cooked mesh assets |
| Content/Textures | Cooked texture assets |
| Content/Audio | Packaged audio assets |
| Content/Scripts | Packaged script assets |

Directory creation does not empty existing output.

The output root also contains RuntimeManifest.json and the persistent
.quiet-cook-cache.json file after successful writes.

## Initial cache loading

main() reads the previous cache from the output root.

If LoadCookCache() returns false, the CLI prints a warning and replaces
the previous cache with an empty cache. This does not abort cooking.

NextCache is assembled separately rather than modifying PreviousCache.

Detailed cache validation and reuse rules are covered with the asset
stages.

## Cache representation

CookCache stores entries by asset ID.

Each entry records:

| Field | Responsibility |
| --- | --- |
| Type | Asset type |
| SourceHash | Fingerprint of the source file |
| CookedHash | Fingerprint of the produced file |
| CookSignature | Stage-defined conversion configuration identifier |
| RuntimePath | Expected package-relative output path |

CookCacheEntry::IsValid() requires a non-Unknown type and nonempty
strings for the remaining fields. It does not inspect files or verify
hashes itself.

### File fingerprints

ComputeFileHash() reads file bytes in 64 KiB chunks and computes a
64-bit FNV-1a hash, represented as sixteen hexadecimal characters.

Open or read failure returns false with an error description.

The function fingerprints only the supplied file. It does not discover
or hash external files referenced by that source.

### Cache loading

LoadCookCache() resets its output before loading.

A missing cache file is accepted as an empty cache.
An incompatible cache version is also accepted as an empty cache.

For the current version, the Assets member must be an object.
Entries require a recognized type, a nonempty asset ID, and valid
entry fields.

Malformed JSON or invalid entry data returns false. Entries are loaded
into a temporary cache and published only after successful validation.

The CLI treats a reported load failure as a warning and cooks using
an empty previous cache.

These return paths do not imply that every filesystem exception is
converted into a false result.

### Cache saving

SaveCookCache() creates the parent directory when needed and writes
the current cache version.

Entries with empty asset IDs or invalid fields are omitted.

The destination is opened with truncation. Saving does not use a
temporary-file replacement, so failure does not guarantee preservation
of the previous cache file.

## Mesh stage

CookProjectMeshes() processes non-built-in Mesh records from the
asset registry.

Relative source paths are resolved against ProjectRoot.
Absolute paths are retained; paths are lexically normalized.

Each output uses:

`Content/Meshes/<asset-ID>.qmesh`

A missing source or failed source fingerprint marks the stage
unsuccessful and processing continues with other mesh records.

The stage returns the accumulated success flag. main() stops the
pipeline if that flag is false.

## Mesh cache reuse

An existing entry is considered for reuse only when all of these match:

- Asset type is Mesh.
- Source hash matches the current source file.
- Cook signature matches the stage's current signature.
- Runtime path matches the expected output path.

Quiet then requires:

1. An available regular cooked file.
2. A cooked-file hash matching the cached hash.
3. Successful decoding through QMeshReader.
4. Nonempty vertices, indices, and submeshes.
5. Valid decoded bounds.

A successful hit appends a runtime asset record and copies the previous
entry into NextCache. It skips import, optimization, and rewriting.

A missing, mismatched, or invalid cached output falls through to
recooking. Reuse checks do not compare against a freshly imported mesh.

## Mesh conversion

On a cache miss, the stage:

1. Imports the source with MeshImporter.
2. Optimizes geometry per submesh.
3. Writes QMesh.
4. Reads the result back and validates it against the optimized mesh.
5. Attempts to fingerprint the produced file.
6. Appends the runtime record.
7. Adds a next-cache entry when output hashing succeeds.

### Geometry optimization

The optimizer validates submesh vertex and index ranges, triangle-list
index counts, and references within each submesh's vertex range.

For nonempty submeshes it runs meshoptimizer's vertex-cache, overdraw,
and vertex-fetch optimization in that order.

It rebuilds the combined vertex/index arrays and updates submesh offsets
and counts. This stage does not perform triangle-count reduction.

Logged ACMR, overfetch, and overdraw values are meshoptimizer analysis
metrics, rather than measurements of Quiet's rendered frame time.

### Round-trip checks

The written file is decoded through QMeshReader and compared with
the optimized mesh.

Checks include geometry counts, triangle topology with cyclic index
rotation allowed, submesh ranges and material slots, and bounds.

Position error is checked against a bound derived from 16-bit
quantization steps. Color error must remain within approximately
1/255, and normal angular error must not exceed 0.1 degrees.

Maximum UV error is measured and logged, but this validator does not
apply a UV-error rejection threshold.

Detailed QMesh packing and compression are documented separately.

### Failure boundaries

Import, optimization, writing, or round-trip validation failure marks
the stage unsuccessful and skips publishing that mesh's runtime record.

A failed validation can leave a written file in the output directory.
The stage does not roll back earlier writes.

Output fingerprint failure after successful conversion is a warning:
the runtime record is retained, but no next-cache entry is added.

## Texture stage

CookProjectTextures() processes non-built-in Texture records.

Source paths are resolved against ProjectRoot when relative.
Each output uses:

`Content/Textures/<asset-ID>.qtexture`

The stage continues after reported per-asset failures and returns false
if any required operation failed.

All converted textures use BC7 sRGB. This stage does not select a linear
format for normal maps, masks, or other data textures.

## Texture cache reuse

Reuse requires matching asset type, source hash, cook signature,
and expected runtime path.

The cooked file must also:

- Exist as an available regular file.
- Match the cached cooked-file hash.
- Load successfully through QTextureReader.
- Pass QTextureData::IsValid().
- Contain a KTX2 payload.
- Report VK_FORMAT_BC7_SRGB_BLOCK.

A successful hit appends the runtime record and copies the previous
entry into NextCache.

Failed reuse falls through to conversion. It does not regenerate the
expected mip chain to compare against the cached output.

## Texture conversion

On a cache miss, the stage:

1. Imports source pixels through TextureImporter.
2. Builds an RGBA8 sRGB mip chain.
3. Compresses each mip to BC7 using Compressonator.
4. Writes the QTexture file.
5. Validates the written payload through QTextureReader and libktx.
6. Attempts output hashing.
7. Publishes the runtime record and, when hashing succeeds, cache metadata.

The production compression quality argument is 0.05.
It is an encoder setting, not a measured image-quality score.

### Mip generation

BuildRgba8SrgbMipChain() requires valid four-channel source data.

The base level copies the imported pixels. Each subsequent level halves
each dimension using integer division, with a minimum of one.
Generation continues until both dimensions are one.

Downsampling uses area-weighted filtering, including odd-sized levels.

RGB is converted from sRGB to linear values before filtering.
The filter accumulates alpha-weighted linear RGB, then unpremultiplies
the result and converts RGB back to sRGB.

Alpha is averaged separately. This prevents fully transparent pixels'
arbitrary RGB values from dominating lower mip levels.

### BC7 blocks

BC7 compression operates on 4-by-4 texel blocks, producing sixteen
bytes per block.

Incomplete edge blocks repeat the nearest edge texels.
Logical mip dimensions remain unchanged.

### Packaging

WriteQTexture() stores a KTX2 BC7 sRGB payload inside Quiet's asset
container.

The texture layout is two-dimensional, with one layer and one face.
The current output uses neither KTX2 supercompression nor outer
QAsset compression.

Detailed container fields belong in the file-format reference.

## Texture validation

Freshly written output is checked against the generated BC7 mip chain.

QTextureReader must preserve:

- Payload and Vulkan formats.
- Base dimensions.
- Mip count.
- Each mip's dimensions and compressed block bytes.

The embedded KTX2 is additionally parsed through libktx.
Validation checks its dimensions, levels, layer and face counts,
absence of supercompression, and each mip's byte size, range, and data.

These checks establish container and compressed-data preservation.
They do not compare BC7-decoded pixels with the original source image.

### Codec startup check

ValidateCompressonatorBC7() creates encoder options and compresses and
decompresses one synthetic RGBA8 block.

This check uses quality 0.5, independently of the production stage's
quality 0.05.

It checks reported codec-operation success, without testing a
reconstruction-error threshold.

## Texture failure boundaries

Import, mip generation, compression, writing, or round-trip validation
failure marks the stage unsuccessful and skips that asset's runtime
record.

A written file can remain after validation failure. Earlier filesystem
changes are not rolled back.

Output hashing failure after successful conversion is a warning.
The runtime record is retained, but no next-cache entry is added.

## Native-file packaging

Audio and script stages process non-built-in records of their
respective asset types.

Relative source paths are resolved against ProjectRoot.
Absolute paths are retained and paths are lexically normalized.

Both stages copy source files without transcoding or wrapping them
in QAsset containers.

| Stage | Accepted extensions | Runtime path |
| --- | --- | --- |
| Audio | .wav, .flac, .mp3 | `Content/Audio/<asset-ID><extension>` |
| Script | .lua | `Content/Scripts/<asset-ID>.lua` |

Extension checks are case-insensitive. Output extensions are lowercase.

Missing source files, unsupported extensions, and source hashing
failures mark the stage unsuccessful. Processing continues with
other records.

## Audio packaging

CookProjectAudio() preserves the source encoding, channels, and sample
rate by copying the file rather than transcoding it.

### Audio inspection

InspectAudioFile() requires a supported extension and successful
miniaudio decoder initialization.

It reads format metadata, requires nonzero channel count and sample
rate, releases the decoder, and obtains the file size.

The reported encoding label is derived from the extension.

This inspection does not decode and validate every audio frame or
calculate the complete duration.

### Audio cache reuse

A cache candidate must match:

- Audio asset type.
- Source hash.
- The audio-native-copy-v1 signature.
- Expected runtime path.

The output must be an available regular file whose hash matches both
the cached cooked hash and the current source hash.

The packaged output must also pass audio inspection.

Successful reuse appends the runtime record and copies the cache entry
into NextCache. Failed reuse falls through to packaging.

### Fresh audio packaging

On a cache miss, the stage:

1. Inspects the source audio.
2. Copies it with overwrite_existing.
3. Hashes the packaged file.
4. Requires its hash to equal the source hash.
5. Inspects the packaged audio.
6. Appends a runtime record and next-cache entry.

Any reported required-operation failure skips publication for that
asset and marks the stage unsuccessful.

## Script packaging

CookProjectScripts() copies registered .lua files without compiling
them to bytecode.

### Script cache reuse

A cache candidate must match the Script type, source hash,
script-lua-text-copy-v1 signature, and expected runtime path.

The output must be an available regular file whose hash matches both
the cached cooked hash and the current source hash.

Successful reuse appends the runtime record and copies the entry into
NextCache. Failed reuse falls through to copying.

### Fresh script packaging

On a cache miss, the stage copies with overwrite_existing, hashes the
result, and requires equality with the source hash.

Only then does it append the runtime record and next-cache entry.

This stage does not parse Lua, execute the file, discover properties,
or validate callback declarations. A successful package operation
does not establish that ScriptWorld can load the script successfully.

File-size inspection is used for reporting; failure prints zero rather
than failing an otherwise successful package operation.

## Native packaging failure boundaries

Both stages return an accumulated success flag and can leave copied
files behind after later validation fails.

Their hash comparisons establish fingerprint equality rather than
performing a separate byte-by-byte comparison.

Unlike mesh and texture conversion, failure to hash a freshly packaged
output is a required-operation failure, not a cache-only warning.

These stages do not roll back filesystem changes.

## Writer responsibilities

The writers serialize prepared data. Import, geometry optimization,
mip generation, compression, and read-back validation remain separate
operations in the cooking stages.

Both writers currently require a little-endian target through a
compile-time assertion.

### QMesh writing

WriteQMesh() requires a successful mesh import, nonempty vertices,
indices and submeshes, valid bounds, and counts within the format's
32-bit limits.

It packs vertex attributes using:

| Attribute | Stored representation |
| --- | --- |
| Position | 16-bit quantization relative to mesh bounds |
| Normal | Two signed 16-bit octahedral components |
| RGB color | 8-bit normalized components |
| Stored alpha | 255 |
| Texture coordinates | Half-precision components |

The packed vertices and indices are encoded with meshoptimizer.
Encoding versions are configured to version 1 once.

The payload also contains mesh bounds and submesh metadata.

WriteQMesh() attempts Zstandard compression at the default level.
It stores the compressed payload only when it is smaller than the
uncompressed payload, setting the corresponding QAsset flag.

A reported compression failure fails writing rather than falling back
to an uncompressed payload.

The caller must prepare the output directory.
The writer opens the destination with truncation.

### BC7 compression helper

CompressRgba8MipChainToBC7() accepts a nonempty collection of valid
RGBA8 mip levels and a quality argument in the documented range 0–1.

It checks each mip's dimensions and pixel byte count. It does not
verify the progression of dimensions between levels.

The output preserves logical dimensions and stores sixteen bytes per
4-by-4 block, repeating edge texels for incomplete blocks.

The helper resets outputs and statistics before processing.
Reported block-encoding or generated-level failures clear them again.

The quality range check compares against zero and one; it does not
explicitly reject NaN before passing quality to the codec.
The production stage supplies the finite constant 0.05.

### QTexture writing

WriteQTexture() requires nonempty, valid BC7 mip levels.
Each supplied level must have the expected dimensions obtained by
halving the base dimensions, with a minimum of one.

That dimension check does not itself require the supplied list to
reach 1-by-1. The production stage supplies the complete chain
generated by BuildRgba8SrgbMipChain().

The writer constructs and serializes KTX2 in memory, then embeds it
inside QTexture and the QAsset container.

It creates the output parent directory when needed and opens the
destination with truncation.

### Results and failures

Writer statistics describe the produced byte representations and
are populated after the checked writes succeed.

Neither writer uses atomic temporary-file replacement or removes
partial output on failure.

WriteQMesh() resets statistics but does not clear outError on entry.
Callers should use its boolean result rather than treating an existing
error string as evidence of failure.

WriteQTexture() resets statistics and clears outError on entry.

Successful writing does not establish a successful read-back round trip.
CookProjectMeshes() and CookProjectTextures() perform that validation
before publishing runtime records.

## Finalization boundary

FinalizeRuntimePackage() copies the startup scene into Scenes using
the descriptor's StartupScene path and overwrites an existing copy.

It assembles the runtime manifest from the project name, packaged
startup scene path, and collected asset records.

RuntimeAssets is moved from CookContext into the manifest.

Failure to prepare the scene directory, copy the scene, or write the
manifest returns false.

After writing the manifest, finalization attempts stale-asset pruning
and cache saving. Failures in those operations become warning strings
in PackageBuildResult rather than a false finalization result.

Successful finalization therefore does not guarantee that cleanup or
cache persistence succeeded.

## Stale-output cleanup

PruneStaleCookedAssets() builds a set of normalized runtime paths from
the manifest's Mesh, Texture, Audio, and Script records.

It recursively scans these output directories:

| Directory | Eligible files |
| --- | --- |
| Content/Meshes | Exact .qmesh extension |
| Content/Textures | Exact .qtexture extension |
| Content/Audio | .wav, .flac, or .mp3, checked case-insensitively |
| Content/Scripts | Exact .lua extension |

Eligible regular files whose package-relative paths are absent from
the manifest are collected for deletion.

Scenes, unsupported file extensions, and empty directories are not
removed by this function.

### Enumeration and deletion

The function collects candidates before deleting them. A reported
enumeration failure returns false before the deletion pass.

Directory traversal uses skip_permission_denied, so successful
completion does not guarantee inspection of every inaccessible location.

During deletion, a failed removal returns false immediately.
Previously removed files are not restored. Cleanup statistics record
successful removals before the failure.

FinalizeRuntimePackage() converts a cleanup failure into a warning
and still attempts to save the next cache.

## Manifest validation

RuntimeManifestIO validates manifests when saving and loading.

Validation requires the current version, a nonempty project name,
valid asset records, unique asset IDs, and normalized portable
package-relative paths.

Paths reject absolute forms, dot or parent segments, and the
implementation's prohibited characters.

Validation checks representation rather than the existence of every
referenced file. It is not a physical containment check through
filesystem symlinks.

Loaded and saved asset lists are sorted by asset ID.

Detailed JSON fields belong in the file-format reference.

## Runtime package consumption

When Application receives a runtime package path, it:

1. Resolves and checks the package directory.
2. Loads RuntimeManifest.json.
3. Initializes ProjectContext from the package.
4. Loads built-in runtime support assets.
5. Loads packaged meshes, textures, and audio.
6. Loads the manifest's startup Scene.

A reported failure in these startup operations throws a runtime error.

The package context uses Content as its asset directory.
It builds a registry from built-in records and manifest records rather
than requiring an editable .quiet project descriptor.

### Resource loading

RuntimeAssetLoader borrows resource libraries and Vulkan dependencies.

It decodes meshes through QMeshReader and textures through
QTextureReader, then creates or replaces resources under their asset IDs.

Audio files are inspected and registered through AudioLibrary with
replacement enabled. Runtime sounds are created later by AudioWorld.

Each selected-type batch continues after reported per-record failures
and returns an accumulated success flag. Previously loaded resources
are not rolled back.

Scripts have no separate eager loading batch in RuntimeAssetLoader.
Their manifest records enter ProjectContext's registry, and ScriptWorld
resolves and executes assigned scripts when Scene runtime begins.

Runtime loading does not consult .quiet-cook-cache.json or recheck its
fingerprints. Reader validation and runtime initialization remain the
loading boundary.

See [Assets](Assets.md), [Application](Application.md), and
[Scripting](Scripting.md) for the detailed runtime contracts.

## Package scope and limitations

Cooking processes registered assets rather than deriving a minimal
dependency set from the startup Scene.

Finalization copies the startup Scene. It does not package every Scene
in the source project.

The output directory contains assets, a startup Scene, and metadata.
This pipeline does not copy the Sandbox executable or assemble a
complete executable distribution.

Built-in assets are excluded from project cooking and supplied through
runtime support loading.

Cooking and finalization are not transactional. A failed invocation can
leave a mixture of earlier outputs, new files, and previous metadata.
Use the final result and diagnostics rather than directory presence as
evidence of a completed cook.