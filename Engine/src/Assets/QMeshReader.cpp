#include "Quiet/Assets/QMeshReader.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>
#include <span>

#include <meshoptimizer.h>
#include <zstd.h>

#include "Quiet/Assets/QAssetFormat.h"

namespace Quiet::Assets
{
    namespace
    {
        template <typename T>
        bool ReadValue(const std::span<const std::uint8_t> bytes, std::size_t& offset, T& outValue)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) return false;

            std::memcpy(&outValue, bytes.data() + offset, sizeof(T));
            offset += sizeof(T);
            return true;
        }

        bool ReadBytes(const std::span<const std::uint8_t> bytes, std::size_t& offset, void* destination, const std::size_t byteCount)
        {
            if (offset > bytes.size() || byteCount > bytes.size() - offset) return false;
            if (byteCount > 0)
            {
                std::memcpy(destination, bytes.data() + offset, byteCount);
            }

            offset += byteCount;
            return true;
        }
    }

    bool QMeshReader::LoadFromFile(const std::filesystem::path& path, QMeshData& outMesh, std::string& outError)
    {
        static_assert(std::endian::native == std::endian::little, "QAsset v1 currently supports little-endian targets only.");
        outError.clear();

        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            outError = "Failed to open QMesh file.";
            return false;
        }

        const std::streampos endPosition = input.tellg();
        if (endPosition <= 0)
        {
            outError = "QMesh file is empty.";
            return false;
        }

        const auto fileSize = static_cast<std::uint64_t>(endPosition);
        if (fileSize > std::numeric_limits<std::size_t>::max())
        {
            outError = "QMesh file exceeds addressable memory size.";
            return false;
        }

        std::vector<std::uint8_t> bytes(fileSize);
        input.seekg(0, std::ios::beg);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input)
        {
            outError = "Failed to read QMesh file.";
            return false;
        }

        std::size_t fileOffset = 0;

        // QAsset header
        std::array<char, 4> magic{};
        if (!ReadBytes(bytes, fileOffset, magic.data(), magic.size()))
        {
            outError = "QMesh is missing its QAsset header.";
            return false;
        }

        if (!std::equal(magic.begin(), magic.end(), QAssetMagic.begin()))
        {
            outError = "Invalid QAsset magic.";
            return false;
        }

        std::uint16_t containerVersion = 0;
        std::uint16_t assetType = 0;

        std::uint32_t payloadVersion = 0;
        std::uint32_t flags = 0;

        std::uint64_t payloadSize = 0;

        if (!ReadValue(bytes, fileOffset, containerVersion) ||
            !ReadValue(bytes, fileOffset, assetType) ||
            !ReadValue(bytes, fileOffset, payloadVersion) ||
            !ReadValue(bytes, fileOffset, flags) ||
            !ReadValue(bytes, fileOffset, payloadSize))
        {
            outError = "Invalid QAsset header.";
            return false;
        }

        if (containerVersion != CurrentQAssetContainerVersion)
        {
            outError = "Unsupported QAsset container version.";
            return false;
        }

        if (assetType != static_cast<std::uint16_t>(QAssetPayloadType::Mesh))
        {
            outError = "QAsset is not a mesh.";
            return false;
        }

        if (payloadVersion != CurrentQMeshPayloadVersion)
        {
            outError = "Unsupported QMesh payload version.";
            return false;
        }

        if ((flags & ~QAssetKnownFlags) != 0)
        {
            outError = "QMesh contains unsupported QAsset flags.";
            return false;
        }

        if (payloadSize != bytes.size() - fileOffset)
        {
            outError = "QMesh payload size does not match file size.";
            return false;
        }

        if (payloadSize == 0)
        {
            outError = "QMesh contains an empty payload.";
            return false;
        }

        const std::span<const std::uint8_t> storedPayload
        {
            bytes.data() + fileOffset, static_cast<std::size_t>(payloadSize)
        };

        std::vector<std::uint8_t> decompressedPayload;
        std::span<const std::uint8_t> payload = storedPayload;

        if ((flags & QAssetFlagZstdCompressed) != 0)
        {
            const unsigned long long decodedSize = ZSTD_getFrameContentSize(storedPayload.data(), storedPayload.size());

            if (decodedSize == ZSTD_CONTENTSIZE_ERROR)
            {
                outError = "QMesh contains an invalid Zstandard frame.";
                return false;
            }

            if (decodedSize == ZSTD_CONTENTSIZE_UNKNOWN)
            {
                outError = "QMesh Zstandard payload does not contain its decoded size.";
                return false;
            }

            constexpr std::uint64_t MaxDecodedQMeshPayload = 2ull * 1024ull * 1024ull * 1024ull;
            if (decodedSize == 0 || decodedSize > MaxDecodedQMeshPayload || decodedSize > std::numeric_limits<std::size_t>::max())
            {
                outError = "QMesh decoded payload size is invalid.";
                return false;
            }

            decompressedPayload.resize(static_cast<std::size_t>(decodedSize));

            const std::size_t decompressedSize =
                ZSTD_decompress(
                    decompressedPayload.data(),
                    decompressedPayload.size(),
                    storedPayload.data(),
                    storedPayload.size()
                );

            if (ZSTD_isError(decompressedSize))
            {
                outError = std::string("Failed to decompress QMesh payload: ") + ZSTD_getErrorName(decompressedSize);
                return false;
            }

            if (decompressedSize != decompressedPayload.size())
            {
                outError = "QMesh decompressed payload size does not match its frame.";
                return false;
            }

            payload = decompressedPayload;
        }

        // QMesh header
        std::uint32_t vertexFormat = 0;
        std::uint32_t vertexStride = 0;

        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t submeshCount = 0;

        std::uint32_t boundsValid = 0;
        std::size_t payloadOffset = 0;

        if (!ReadValue(payload, payloadOffset, vertexFormat) ||
            !ReadValue(payload, payloadOffset, vertexStride) ||
            !ReadValue(payload, payloadOffset, vertexCount) ||
            !ReadValue(payload, payloadOffset, indexCount) ||
            !ReadValue(payload, payloadOffset, submeshCount) ||
            !ReadValue(payload, payloadOffset, boundsValid))
        {
            outError = "Invalid QMesh geometry header.";
            return false;
        }

        if (vertexFormat != static_cast<std::uint32_t>(QMeshVertexFormat::Float32PositionColorNormalTexCoord))
        {
            outError = "Unsupported QMesh vertex format.";
            return false;
        }

        if (vertexStride != sizeof(QMeshVertexV1))
        {
            outError = "Unexpected QMesh vertex stride.";
            return false;
        }

        if (vertexCount == 0 || indexCount == 0 || submeshCount == 0)
        {
            outError = "QMesh contains empty geometry.";
            return false;
        }

        if (indexCount % 3 != 0)
        {
            outError = "QMesh index buffer is not a triangle list.";
            return false;
        }

        if (boundsValid > 1)
        {
            outError = "QMesh contains an invalid bounds flag.";
            return false;
        }

        std::array<float, 6> bounds{};

        if (!ReadBytes(payload, payloadOffset, bounds.data(), sizeof(bounds)))
        {
            outError = "Invalid QMesh bounds.";
            return false;
        }

        std::uint64_t vertexStreamSize = 0;
        std::uint64_t indexStreamSize = 0;

        if (!ReadValue(payload, payloadOffset, vertexStreamSize) ||
            !ReadValue(payload, payloadOffset, indexStreamSize))
        {
            outError = "Invalid QMesh stream information.";
            return false;
        }

        if (vertexStreamSize == 0 || indexStreamSize == 0)
        {
            outError = "QMesh contains empty encoded streams.";
            return false;
        }

        // Submeshes
        constexpr std::size_t submeshRecordSize = sizeof(std::uint32_t) * 5;
        if (submeshCount > (payload.size() - payloadOffset) / submeshRecordSize)
        {
            outError = "QMesh submesh table exceeds file size.";
            return false;
        }

        std::vector<QMeshSubmeshV1> submeshes;
        submeshes.reserve(submeshCount);

        for (std::uint32_t i = 0; i < submeshCount; ++i)
        {
            QMeshSubmeshV1 submesh{};
            if (!ReadValue(payload, payloadOffset, submesh.BaseVertex) ||
                !ReadValue(payload, payloadOffset, submesh.VertexCount) ||
                !ReadValue(payload, payloadOffset, submesh.BaseIndex) ||
                !ReadValue(payload, payloadOffset, submesh.IndexCount) ||
                !ReadValue(payload, payloadOffset, submesh.MaterialSlot))
            {
                outError = "Invalid QMesh submesh table.";
                return false;
            }

            submeshes.push_back(submesh);
        }

        // Encoded streams
        const std::uint64_t remainingBytes = payload.size() - payloadOffset;

        if (vertexStreamSize > remainingBytes || indexStreamSize > remainingBytes - vertexStreamSize)
        {
            outError = "QMesh encoded streams exceed payload size.";
            return false;
        }

        if (vertexStreamSize + indexStreamSize != remainingBytes)
        {
            outError = "QMesh contains unexpected trailing data.";
            return false;
        }

        const auto* encodedVertices = payload.data() + payloadOffset;
        const auto* encodedIndices = encodedVertices + static_cast<std::size_t>(vertexStreamSize);

        if (meshopt_decodeVertexVersion(encodedVertices, static_cast<std::size_t>(vertexStreamSize)) != 1)
        {
            outError = "Unsupported meshoptimizer vertex codec version.";
            return false;
        }

        if (meshopt_decodeIndexVersion(encodedIndices, static_cast<std::size_t>(indexStreamSize)) != 1)
        {
            outError = "Unsupported meshoptimizer index codec version.";
            return false;
        }

        std::vector<QMeshVertexV1> diskVertices(vertexCount);

        if (meshopt_decodeVertexBuffer(diskVertices.data(), diskVertices.size(), sizeof(QMeshVertexV1), encodedVertices, static_cast<std::size_t>(vertexStreamSize)) != 0)
        {
            outError = "Failed to decode QMesh vertex stream.";
            return false;
        }

        std::vector<MeshIndex> indices(indexCount);

        if (meshopt_decodeIndexBuffer(
            indices.data(),
            indices.size(),
            sizeof(MeshIndex),
            encodedIndices,
            static_cast<std::size_t>(indexStreamSize)) != 0)
        {
            outError = "Failed to decode QMesh index stream.";
            return false;
        }

        // meshoptimizer's decoder validates the stream itself,
        // but decoded indices still need semantic range validation.
        for (const MeshIndex index : indices)
        {
            if (index >= vertexCount)
            {
                outError = "QMesh contains an out-of-range vertex index.";
                return false;
            }
        }

        for (const QMeshSubmeshV1& submesh : submeshes)
        {
            const std::uint64_t vertexEnd = static_cast<std::uint64_t>(submesh.BaseVertex) + submesh.VertexCount;
            const std::uint64_t indexEnd = static_cast<std::uint64_t>(submesh.BaseIndex) + submesh.IndexCount;

            if (vertexEnd > vertexCount || indexEnd > indexCount)
            {
                outError = "QMesh contains an invalid submesh range.";
                return false;
            }

            if (submesh.IndexCount % 3 != 0)
            {
                outError = "QMesh submesh is not a triangle list.";
                return false;
            }

            for (std::uint64_t i = submesh.BaseIndex; i < indexEnd; ++i)
            {
                const MeshIndex index = indices[static_cast<std::size_t>(i)];
                if (index < submesh.BaseVertex || index >= vertexEnd)
                {
                    outError = "QMesh submesh references a vertex outside its range.";
                    return false;
                }
            }
        }

        QMeshData result{};

        result.Vertices.reserve(diskVertices.size());

        for (const QMeshVertexV1& source : diskVertices)
        {
            MeshVertex vertex{};

            vertex.Position =
            {
                source.Position[0],
                source.Position[1],
                source.Position[2]
            };

            vertex.Color =
            {
                source.Color[0],
                source.Color[1],
                source.Color[2]
            };

            vertex.Normal =
            {
                source.Normal[0],
                source.Normal[1],
                source.Normal[2]
            };

            vertex.TexCoord =
            {
                source.TexCoord[0],
                source.TexCoord[1]
            };

            result.Vertices.push_back(vertex);
        }

        result.Indices = std::move(indices);
        result.Submeshes = std::move(submeshes);
        result.Bounds.Valid = boundsValid != 0;

        if (result.Bounds.Valid)
        {
            result.Bounds.Min =
            {
                bounds[0],
                bounds[1],
                bounds[2]
            };

            result.Bounds.Max =
            {
                bounds[3],
                bounds[4],
                bounds[5]
            };
        }

        outMesh = std::move(result);
        return true;
    }
}