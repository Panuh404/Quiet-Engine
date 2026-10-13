# Runtime Package Files <!--! {#quiet_format_runtime_package} -->

The package root contains two JSON documents with different purposes:

| File | Consumer | Purpose |
| --- | --- | --- |
| RuntimeManifest.json | Runtime startup | Identifies the startup Scene and packaged assets |
| .quiet-cook-cache.json | Cooker | Records fingerprints and conversion signatures for reuse |

Sandbox does not require the cook cache.

See [Cooking](../Systems/Cooking.md) for production and cleanup behavior.

## Runtime manifest

Implementation:

- Engine/include/Quiet/Project/RuntimeManifest.h
- Engine/src/Project/RuntimeManifest.cpp
- Quiet::Project::RuntimeManifestIO

Example:

```json
{
    "Version": 1,
    "ProjectName": "FirstProject",
    "StartupScene": "Scenes/DefaultScene.json",
    "Assets": [
        {
            "ID": "Crate",
            "Type": "Mesh",
            "Path": "Content/Meshes/Crate.qmesh"
        },
        {
            "ID": "Rotate",
            "Type": "Script",
            "Path": "Content/Scripts/Rotate.lua"
        }
    ]
}
```

The example paths illustrate layout; the corresponding files must exist
for successful runtime use.

### Root fields

All four fields are required by the reader.

| Field | JSON type | Validation |
| --- | --- | --- |
| Version | Integer | Must equal CurrentRuntimeManifestVersion, currently 1 |
| ProjectName | String | Nonempty |
| StartupScene | String | Accepted package-relative path |
| Assets | Array | Every entry must be a valid asset object |

An empty Assets array is accepted.

### Asset fields

All three fields are required.

| Field | JSON type | Validation |
| --- | --- | --- |
| ID | String | Nonempty and unique within the manifest |
| Type | String | Recognized asset type other than Unknown |
| Path | String | Nonempty accepted package-relative path |

Accepted type strings are Mesh, Texture, Audio, and Script.
Type strings are case-sensitive.

The manifest has no BuiltIn field or asset fingerprints.
Built-in assets are supplied separately during runtime initialization.

Validation does not require unique paths, expected file extensions,
or matching Content subdirectories.

### Package-relative paths

StartupScene and asset Path values must:

- Be nonempty.
- Not start with a forward slash.
- Use a representation unchanged by lexical normalization.
- Contain no dot or parent-directory segments.
- Contain none of the prohibited characters: backslash, colon,
  asterisk, question mark, quotation mark, angle brackets, or vertical bar.

Examples:

| Value | Result |
| --- | --- |
| Scenes/DefaultScene.json | Accepted |
| Content/Meshes/Crate.qmesh | Accepted |
| ../Crate.qmesh | Rejected |
| ./Content/Crate.qmesh | Rejected |
| Content/../Crate.qmesh | Rejected |
| C:/Packages/Crate.qmesh | Rejected |

These are lexical representation checks.
They do not inspect file existence or resolve physical containment
through filesystem symlinks.

### Loading

The reader requires an object root and object asset entries.

Missing fields, incompatible field types, malformed JSON, invalid paths,
duplicate IDs, and incompatible versions produce false through the
validation or caught JSON-exception paths.

The manifest is built locally and assigned to outManifest only after
successful validation. Reported failure leaves the previous output
unchanged.

Successful loading sorts assets by ID.

The function catches JSON exceptions, not every possible exception.

### Saving

SaveToFile() validates the supplied manifest before writing.

It sorts a copy by asset ID, creates the parent directory when needed,
and writes indented JSON. The caller's asset ordering is unchanged.

The writer emits only the known root and asset fields.

It writes directly to the destination and returns the stream status
after writing. There is no atomic replacement or checked-close guarantee.

## Cook cache

Implementation:

- Cooker/src/Core/CookCache.h
- Cooker/src/Core/CookCache.cpp
- Quiet::Cooker::LoadCookCache
- Quiet::Cooker::SaveCookCache

Unlike the manifest's asset array, cache Assets is an object keyed by ID.

Illustrative example with placeholder fingerprints:

```json
{
    "Version": 1,
    "Assets": {
        "Rotate": {
            "Type": "Script",
            "SourceHash": "0123456789abcdef",
            "CookedHash": "0123456789abcdef",
            "CookSignature": "script-lua-text-copy-v1",
            "RuntimePath": "Content/Scripts/Rotate.lua"
        }
    }
}
```

These fingerprints are examples, not hashes of a supplied file.

### Root fields

| Field | JSON type | Reader behavior |
| --- | --- | --- |
| Version | Unsigned 32-bit integer | Compared with CurrentCookCacheVersion, currently 1 |
| Assets | Object | Required when the version is compatible |

A missing file is accepted as an empty cache.

A different version is accepted as an empty cache without reading its
asset entries. This differs from manifest version rejection.

### Entry fields

Every field is required.

| Field | JSON type | Meaning |
| --- | --- | --- |
| Type | String | Asset classification |
| SourceHash | String | Source-file fingerprint |
| CookedHash | String | Output-file fingerprint |
| CookSignature | String | Conversion or packaging configuration identifier |
| RuntimePath | String | Expected package-relative output path |

Asset IDs must be nonempty.
Type must be recognized and other than Unknown.
The remaining strings must be nonempty.

The cache reader does not validate hexadecimal hash syntax, fingerprint
length, signature recognition, file existence, or RuntimePath safety.

Those structural checks do not establish a cache hit.
Each cooking stage additionally compares expected metadata and checks
the existing output.

### Fingerprint representation

ComputeFileHash() generates sixteen hexadecimal characters from a
64-bit FNV-1a hash of file bytes.

It reads in 64 KiB chunks and fingerprints only the supplied file.
External dependencies are not automatically included.

Hashes are used for reuse checks; they are not cryptographic signatures.

### Current cook signatures

| Stage | Signature |
| --- | --- |
| Mesh | `qmesh-v1\|meshopt-opt-v1\|meshopt-encode-v1\|quant-pos16-oct16-color8-halfuv\|zstd-default` |
| Texture | `qtexture-v1\|ktx2-bc7-srgb\|mipgen-srgb-premul-v1\|compressonator-bc7-q0.05` |
| Audio | `audio-native-copy-v1` |
| Script | `script-lua-text-copy-v1` |

Signatures are compared as exact strings.
They describe the current stage configuration rather than a complete
fingerprint of every tool or dependency binary.

### Loading and failure

LoadCookCache() resets outCache and clears outError before inspecting
the file.

Malformed JSON, missing compatible-version fields, invalid entry
structure, and reported open failures return false with an error.

Entries are assembled locally and published only after successful
processing. An invalid entry rejects the load rather than being skipped.

Filesystem exceptions are not universally caught.

The CLI warns on a false result and proceeds with an empty previous
cache.

### Saving

SaveCookCache() writes CurrentCookCacheVersion rather than copying the
supplied cache's Version field.

It omits empty asset IDs and entries failing IsValid().
It does not normalize RuntimePath or recompute fingerprints.

The writer creates parent directories as needed and truncates the
destination. It checks stream status after writing without atomic
replacement or a checked-close guarantee.

Finalization treats a reported cache-save failure as a warning.

## Runtime boundary

The manifest identifies resources; it does not embed them.

Successful manifest parsing does not guarantee successful resource,
Scene, audio, or Lua initialization.

See [Cooking guide](../Guides/Cooking.md) for package production and
Sandbox launch, and [Assets](../Systems/Assets.md) for runtime loading.