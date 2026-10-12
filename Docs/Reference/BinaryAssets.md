# Binary Assets

Quiet stores cooked meshes and textures inside a shared QAsset container.

| Extension | Container payload type | Current writer output |
| --- | --- | --- |
| .qmesh | Mesh, value 1 | Quantized and meshoptimizer-encoded geometry |
| .qtexture | Texture, value 2 | Embedded KTX2 containing BC7 sRGB mip data |

Native audio and Lua files do not use this container.

## Implementation map

| Responsibility | Source |
| --- | --- |
| Container constants | Engine/include/Quiet/Assets/QAssetFormat.h |
| Mesh layouts | Engine/include/Quiet/Assets/QMeshFormat.h |
| Texture payload format | Engine/include/Quiet/Assets/QTextureFormat.h |
| Mesh writer | Cooker/src/Mesh/QMeshWriter.cpp |
| Texture writer | Cooker/src/Texture/QTextureWriter.cpp |
| Mesh reader | Engine/src/Assets/QMeshReader.cpp |
| Texture reader | Engine/src/Assets/QTextureReader.cpp |

Offsets below are bytes from the beginning of the indicated structure.

Current readers and writers require little-endian targets through
compile-time assertions. Fields are serialized individually rather than
by writing a padded C++ header structure.

## QAsset header

The header occupies exactly 24 bytes.

| Offset | Size | Representation | Field |
| --- | --- | --- | --- |
| 0 | 4 | ASCII bytes | Magic: QAST |
| 4 | 2 | uint16 | Container version, currently 1 |
| 6 | 2 | uint16 | Payload type: Mesh=1, Texture=2 |
| 8 | 4 | uint32 | Payload version |
| 12 | 4 | uint32 | Flags |
| 16 | 8 | uint64 | Stored payload size |
| 24 | Variable | Bytes | Stored payload |

Payload size excludes the container header.
Readers require it to equal the remaining file size.

### Flags

| Value | Meaning |
| --- | --- |
| 0 | No outer compression |
| 1 | Zstandard-compressed payload |

Bit zero is the only defined flag.

QMesh accepts that flag and rejects unknown bits.
QTexture currently requires flags to be zero, even though the shared
container defines Zstandard compression.

Both current payload versions are 1.
Readers reject unsupported container versions, payload versions,
and mismatched payload types.

## QMesh stored payload

With flags zero, the stored payload is the decoded mesh payload directly.

With flag one, it is a Zstandard frame containing that decoded payload.

The writer uses the default Zstandard compression level and selects
compression only when its result is smaller.

The reader requires the frame to report a known, nonzero decoded size.
Its decoded-size limit is 2 GiB, additionally bounded by addressable
memory. Decompression must produce the reported size.

That limit belongs to compressed-payload decoding; it is not a universal
file-size limit.

## QMesh decoded layout

The fixed prefix occupies 64 bytes.

| Offset | Size | Representation | Field |
| --- | --- | --- | --- |
| 0 | 4 | uint32 | Vertex format |
| 4 | 4 | uint32 | Decoded disk-vertex stride |
| 8 | 4 | uint32 | Vertex count |
| 12 | 4 | uint32 | Index count |
| 16 | 4 | uint32 | Submesh count |
| 20 | 4 | uint32 | Bounds-valid flag: 0 or 1 |
| 24 | 12 | Three float32 values | Minimum bounds: x, y, z |
| 36 | 12 | Three float32 values | Maximum bounds: x, y, z |
| 48 | 8 | uint64 | Encoded vertex-stream size |
| 56 | 8 | uint64 | Encoded index-stream size |
| 64 | 20 × submesh count | Submesh records | Submesh table |
| After table | Vertex-stream size | Encoded bytes | Vertex stream |
| After vertex stream | Index-stream size | Encoded bytes | Index stream |

Stream sizes describe encoded bytes, not decoded array sizes.
The streams must consume the payload's remaining bytes exactly.

### Submesh record

Each record occupies 20 bytes.

| Offset | Size | Representation | Field |
| --- | --- | --- | --- |
| 0 | 4 | uint32 | BaseVertex |
| 4 | 4 | uint32 | VertexCount |
| 8 | 4 | uint32 | BaseIndex |
| 12 | 4 | uint32 | IndexCount |
| 16 | 4 | uint32 | MaterialSlot |

Indices refer to the combined vertex array.
Submesh references must lie within that submesh's vertex range.

QMesh does not store source submesh names or complete material objects.

## Mesh vertex formats

Both formats are supported by the reader.
The current writer emits format 2.

### Format 1: Float32PositionColorNormalTexCoord

Decoded disk stride: 44 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 12 | float32 Position[3] |
| 12 | 12 | float32 Color[3] |
| 24 | 12 | float32 Normal[3] |
| 36 | 8 | float32 TexCoord[2] |

These records are still contained in the encoded vertex stream rather
than written as a raw array after the submesh table.

### Format 2: QuantizedPosition16OctNormal16Color8HalfTexCoord

Decoded disk stride: 20 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 6 | uint16 Position[3], bounds-relative UNORM16 |
| 6 | 2 | uint16 PositionPadding |
| 8 | 4 | int16 NormalOct[2], octahedral SNORM16 |
| 12 | 4 | uint8 Color[4], normalized RGB and reserved alpha |
| 16 | 4 | uint16 TexCoordHalf[2], half-precision bit patterns |

The current writer zero-initializes padding and writes alpha as 255.
The runtime MeshVertex uses RGB; reserved alpha is not copied into it.

Position reconstruction uses:

minimum + (quantized value / 65535) × (maximum - minimum)

Normals are reconstructed from the octahedral representation.
UV components are decoded from half precision.

## Mesh codecs and validation

Both vertex and index streams use meshoptimizer codec version 1.

Indices decode to 32-bit values.

The reader checks:

- Supported vertex format and matching stride.
- Nonzero geometry counts.
- Triangle-list index counts.
- Bounds flag restricted to zero or one.
- Submesh table and stream ranges.
- Exact consumption of remaining payload bytes.
- Supported codec versions and successful decoding.
- Global index bounds.
- Submesh ranges and references within each submesh.

Quantized vertices require valid bounds.

These checks are not equivalent to the Cooker's comparison against the
optimized source mesh or a general finite-value check of all attributes.

## QTexture payload

The uncompressed payload begins with a 12-byte prefix.

| Offset | Size | Representation | Field |
| --- | --- | --- | --- |
| 0 | 4 | uint32 | Payload format: Ktx2=1 |
| 4 | 8 | uint64 | Embedded KTX2 byte count |
| 12 | Variable | Bytes | Embedded KTX2 |

The embedded count must match the remaining payload size.
A standalone KTX2 file is not a complete QTexture file.

## Embedded KTX2 layout

QTextureReader parses KTX2 directly without requiring libktx.
The Cooker additionally uses libktx for independent round-trip checking.

The embedded header occupies 80 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 12 | KTX2 identifier |
| 12 | 4 | vkFormat |
| 16 | 4 | typeSize |
| 20 | 4 | pixelWidth |
| 24 | 4 | pixelHeight |
| 28 | 4 | pixelDepth |
| 32 | 4 | layerCount |
| 36 | 4 | faceCount |
| 40 | 4 | levelCount |
| 44 | 4 | supercompressionScheme |
| 48 | 4 | Data Format Descriptor offset |
| 52 | 4 | Data Format Descriptor length |
| 56 | 4 | Key/value data offset |
| 60 | 4 | Key/value data length |
| 64 | 8 | Supercompression global-data offset |
| 72 | 8 | Supercompression global-data length |
| 80 | 24 × levelCount | Level index |

Each level-index entry contains three uint64 values:

- ByteOffset.
- ByteLength.
- UncompressedByteLength.

ByteOffset is relative to the beginning of the embedded KTX2.
Resolve mip locations through this index rather than assuming sequential
storage in logical mip order.

### Accepted layout

The reader requires:

- A matching KTX2 identifier.
- Nonzero Vulkan format, width, and height.
- pixelDepth=0.
- layerCount=0, representing a non-array texture.
- faceCount=1.
- At least one explicit mip level.
- supercompressionScheme=0.
- Nonempty, in-range Data Format Descriptor data.
- In-range key/value data when present.
- Zero supercompression global-data offset and length.
- Nonempty, in-range mip data with equal stored and uncompressed lengths.

The writer's libktx creation settings describe one non-array layer.
Its serialized layerCount is zero; these are different representations.

The reader does not require BC7 specifically. The current Cooker emits
VK_FORMAT_BC7_SRGB_BLOCK and validates that format in its cooking stage.

### Mip data

Logical dimensions are derived by halving the base dimensions for each
level, with a minimum of one.

BC7 stores sixteen bytes per 4-by-4 block:

ceil(width / 4) × ceil(height / 4) × 16

The compressor repeats edge texels for incomplete blocks without changing
the logical dimensions.

QTextureReader preserves compressed mip bytes rather than decoding them
to RGBA pixels. Its generic validity checks do not verify BC7 byte counts
or measure image reconstruction quality.

## Reader results and persistence boundaries

Both readers clear the error string before loading.

QMeshReader leaves the supplied output unchanged on reported failure
and replaces it after successful decoding.

QTextureReader resets the supplied output before loading, so reported
failure leaves it empty.

Reported validation failures return false with an error description.

Reader success does not establish successful GPU resource creation.

Neither format embeds the project's asset ID, source path, cook-cache
fingerprints, or conversion signature. Runtime manifest records supply
identity and paths.

Writers directly truncate output files and do not provide atomic
replacement. Cooking stages perform their own read-back validation.

See [Cooking](../Systems/Cooking.md) for conversion and validation
and [Runtime package files](RuntimePackageFiles.md) for manifest identity.