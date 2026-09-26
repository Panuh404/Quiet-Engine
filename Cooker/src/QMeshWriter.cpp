#include "QMeshWriter.h"

#include <bit>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <cmath>

#include <zstd.h>
#include <meshoptimizer.h>

#include "Quiet/Assets/QAssetFormat.h"
#include "Quiet/Assets/QMeshFormat.h"

namespace Quiet::Cooker
{
    namespace
    {
        constexpr std::uint64_t QAssetHeaderSize =
            4u +
            sizeof(std::uint16_t) +
            sizeof(std::uint16_t) +
            sizeof(std::uint32_t) +
            sizeof(std::uint32_t) +
            sizeof(std::uint64_t);

        template <typename T> void AppendValue(std::vector<std::uint8_t>& bytes, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), first, first + sizeof(T));
        }

        void AppendBytes(std::vector<std::uint8_t>& bytes, const void* data, const std::size_t byteCount)
        {
            if (byteCount == 0) return;
            const auto* first = static_cast<const std::uint8_t*>(data);
            bytes.insert(bytes.end(), first, first + byteCount);
        }

        template <typename T>
        bool WriteValue(std::ofstream& output, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return output.good();
        }

        bool WriteBytes(std::ofstream& output, const void* bytes, const std::size_t byteCount)
        {
            if (byteCount == 0) return true;
            output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(byteCount));
            return output.good();
        }

        float SignNotZero(const float value)
        {
            return value >= 0.0f ? 1.0f : -1.0f;
        }

        std::uint16_t QuantizePosition(const float value, const float minimum, const float maximum)
        {
            const float extent = maximum - minimum;
            if (extent <= 0.0f) return 0;

            const float normalized = std::clamp((value - minimum) / extent, 0.0f, 1.0f);
            return static_cast<std::uint16_t>(meshopt_quantizeUnorm(normalized, 16));
        }

        void EncodeOctNormal(const Assets::MeshVertex& source, std::int16_t (&output)[2])
        {
            float x = source.Normal.x;
            float y = source.Normal.y;
            float z = source.Normal.z;

            const float length = std::sqrt(x * x + y * y + z * z);
            if (length > 1.0e-20f)
            {
                x /= length;
                y /= length;
                z /= length;
            }
            else
            {
                x = 0.0f;
                y = 0.0f;
                z = 1.0f;
            }

            const float l1 = std::abs(x) + std::abs(y) + std::abs(z);

            x /= l1;
            y /= l1;
            z /= l1;

            if (z < 0.0f)
            {
                const float oldX = x;
                const float oldY = y;

                x = (1.0f - std::abs(oldY)) * SignNotZero(oldX);
                y = (1.0f - std::abs(oldX)) * SignNotZero(oldY);
            }

            output[0] = static_cast<std::int16_t>(meshopt_quantizeSnorm(x, 16));
            output[1] = static_cast<std::int16_t>(meshopt_quantizeSnorm(y, 16));
        }

        std::vector<Assets::QMeshVertexQuantizedV1> BuildQuantizedDiskVertices(const Assets::MeshImportResult& mesh)
        {
            std::vector<Assets::QMeshVertexQuantizedV1> vertices;
            vertices.reserve(mesh.Vertices.size());

            for (const Assets::MeshVertex& source : mesh.Vertices)
            {
                Assets::QMeshVertexQuantizedV1 vertex{};

                vertex.Position[0] = QuantizePosition(source.Position.x, mesh.Bounds.Min.x, mesh.Bounds.Max.x);
                vertex.Position[1] = QuantizePosition(source.Position.y, mesh.Bounds.Min.y, mesh.Bounds.Max.y);
                vertex.Position[2] = QuantizePosition(source.Position.z, mesh.Bounds.Min.z, mesh.Bounds.Max.z);

                EncodeOctNormal(source, vertex.NormalOct);

                vertex.Color[0] = static_cast<std::uint8_t>(meshopt_quantizeUnorm(source.Color.x, 8));
                vertex.Color[1] = static_cast<std::uint8_t>(meshopt_quantizeUnorm(source.Color.y, 8));
                vertex.Color[2] = static_cast<std::uint8_t>(meshopt_quantizeUnorm(source.Color.z, 8));
                vertex.Color[3] = 255;

                vertex.TexCoordHalf[0] = meshopt_quantizeHalf(source.TexCoord.x);
                vertex.TexCoordHalf[1] = meshopt_quantizeHalf(source.TexCoord.y);
                 vertices.push_back(vertex);
            }

            return vertices;
        }

        void ConfigureMeshoptimizerEncoding()
        {
            static std::once_flag configureFlag;
            std::call_once(
                configureFlag,
                []
                {
                    meshopt_encodeVertexVersion(1);
                    meshopt_encodeIndexVersion(1);
                }
            );
        }
    }

    bool WriteQMesh(const std::filesystem::path& path, const Assets::MeshImportResult& mesh, QMeshWriteStats& outStats, std::string& outError)
    {
        static_assert(std::endian::native == std::endian::little, "QAsset v1 currently supports little-endian targets only.");
        outStats = {};
        ConfigureMeshoptimizerEncoding();

        if (!mesh.Succeeded())
        {
            outError = "Cannot write an unsuccessful mesh import.";
            return false;
        }

        if (mesh.Vertices.empty() || mesh.Indices.empty() || mesh.Submeshes.empty())
        {
            outError = "Cannot write an empty mesh.";
            return false;
        }

        if (!mesh.Bounds.Valid)
        {
            outError = "Cannot quantize QMesh without valid mesh bounds.";
            return false;
        }

        if (mesh.Vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
            mesh.Indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            mesh.Submeshes.size() > std::numeric_limits<std::uint32_t>::max())
        {
            outError = "Mesh exceeds QMesh v1 count limits.";
            return false;
        }

        const std::vector<Assets::QMeshVertexQuantizedV1> diskVertices = BuildQuantizedDiskVertices(mesh);
        std::vector<unsigned int> diskIndices;
        diskIndices.reserve(mesh.Indices.size());

        for (const Assets::MeshIndex index : mesh.Indices)
        {
            diskIndices.push_back(static_cast<unsigned int>(index));
        }

        std::vector<unsigned char> encodedVertices(
            meshopt_encodeVertexBufferBound(
                diskVertices.size(),
                sizeof(Assets::QMeshVertexQuantizedV1)));

        const std::size_t encodedVertexSize =
            meshopt_encodeVertexBuffer(
                encodedVertices.data(),
                encodedVertices.size(),
                diskVertices.data(),
                diskVertices.size(),
                sizeof(Assets::QMeshVertexQuantizedV1));

        if (encodedVertexSize == 0)
        {
            outError = "meshoptimizer failed to encode vertex data.";
            return false;
        }

        encodedVertices.resize(encodedVertexSize);

        std::vector<unsigned char> encodedIndices(
            meshopt_encodeIndexBufferBound(
                diskIndices.size(),
                diskVertices.size()));

        const std::size_t encodedIndexSize =
            meshopt_encodeIndexBuffer(
                encodedIndices.data(),
                encodedIndices.size(),
                diskIndices.data(),
                diskIndices.size());

        if (encodedIndexSize == 0)
        {
            outError = "meshoptimizer failed to encode index data.";
            return false;
        }

        encodedIndices.resize(encodedIndexSize);

        // Build the uncompressed QMesh payload.
        std::vector<std::uint8_t> payload;

        payload.reserve(
            128u +
            mesh.Submeshes.size() * sizeof(Assets::QMeshSubmeshV1) +
            encodedVertices.size() +
            encodedIndices.size()
        );

        const std::uint32_t vertexFormat = static_cast<std::uint32_t>(Assets::QMeshVertexFormat::QuantizedPosition16OctNormal16Color8HalfTexCoord);
        const std::uint32_t vertexStride = sizeof(Assets::QMeshVertexQuantizedV1);
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(diskVertices.size());
        const std::uint32_t indexCount = static_cast<std::uint32_t>(diskIndices.size());
        const std::uint32_t submeshCount = static_cast<std::uint32_t>(mesh.Submeshes.size());
        const std::uint32_t boundsValid = mesh.Bounds.Valid ? 1u : 0u;

        AppendValue(payload, vertexFormat);
        AppendValue(payload, vertexStride);
        AppendValue(payload, vertexCount);
        AppendValue(payload, indexCount);
        AppendValue(payload, submeshCount);
        AppendValue(payload, boundsValid);

        const float bounds[6]
        {
            mesh.Bounds.Valid ? mesh.Bounds.Min.x : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Min.y : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Min.z : 0.0f,

            mesh.Bounds.Valid ? mesh.Bounds.Max.x : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Max.y : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Max.z : 0.0f
        };

        AppendBytes(payload, bounds, sizeof(bounds));

        const std::uint64_t vertexStreamSize = encodedVertices.size();
        const std::uint64_t indexStreamSize = encodedIndices.size();

        AppendValue(payload, vertexStreamSize);
        AppendValue(payload, indexStreamSize);

        for (const Assets::ImportedSubmesh& submesh : mesh.Submeshes)
        {
            AppendValue(payload, submesh.BaseVertex);
            AppendValue(payload, submesh.VertexCount);
            AppendValue(payload, submesh.BaseIndex);
            AppendValue(payload, submesh.IndexCount);
            AppendValue(payload, submesh.MaterialIndex);
        }

        AppendBytes(payload, encodedVertices.data(), encodedVertices.size());
        AppendBytes(payload, encodedIndices.data(), encodedIndices.size());

        // Compress the QAsset payload with Zstandard.
        const std::size_t compressionBound = ZSTD_compressBound(payload.size());
        if (ZSTD_isError(compressionBound))
        {
            outError = std::string("Zstandard failed to calculate compression bound: ") + ZSTD_getErrorName(compressionBound);
            return false;
        }

        std::vector<std::uint8_t> compressedPayload(compressionBound);
        const std::size_t compressedSize =
            ZSTD_compress(
                compressedPayload.data(),
                compressedPayload.size(),
                payload.data(),
                payload.size(),
                ZSTD_CLEVEL_DEFAULT
            );

        if (ZSTD_isError(compressedSize))
        {
            outError = std::string("Zstandard failed to compress QMesh payload: ") + ZSTD_getErrorName(compressedSize);
            return false;
        }

        compressedPayload.resize(compressedSize);
        const bool useZstd = compressedPayload.size() < payload.size();

        const std::vector<std::uint8_t>& storedPayload = useZstd ? compressedPayload : payload;
        const std::uint32_t flags = useZstd ? Assets::QAssetFlagZstdCompressed : Assets::QAssetFlagNone;
        const std::uint64_t payloadSize = storedPayload.size();

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            outError = "Failed to open QMesh output file.";
            return false;
        }

        // QAsset header
        if (!WriteBytes(output, Assets::QAssetMagic.data(), Assets::QAssetMagic.size()))
        {
            outError = "Failed to write QAsset magic.";
            return false;
        }

        const std::uint16_t containerVersion = Assets::CurrentQAssetContainerVersion;
        const std::uint16_t assetType = static_cast<std::uint16_t>(Assets::QAssetPayloadType::Mesh);
        const std::uint32_t payloadVersion = Assets::CurrentQMeshPayloadVersion;

        if (!WriteValue(output, containerVersion) ||
            !WriteValue(output, assetType) ||
            !WriteValue(output, payloadVersion) ||
            !WriteValue(output, flags) ||
            !WriteValue(output, payloadSize))
        {
            outError = "Failed to write QAsset header.";
            return false;
        }

        // QMesh payload header
        if (!WriteBytes(output, storedPayload.data(), storedPayload.size()))
        {
            outError = "Failed to write QMesh payload.";
            return false;
        }

        outStats.RawVertexBytes = mesh.Vertices.size() * sizeof(Assets::MeshVertex);
        outStats.PackedVertexBytes = diskVertices.size() * sizeof(Assets::QMeshVertexQuantizedV1);
        outStats.EncodedVertexBytes = encodedVertices.size();
        outStats.RawIndexBytes = diskIndices.size() * sizeof(unsigned int);
        outStats.EncodedIndexBytes = encodedIndices.size();
        outStats.UncompressedPayloadBytes = payload.size();
        outStats.StoredPayloadBytes = storedPayload.size();
        outStats.ZstdCompressed = useZstd;
        outStats.FileBytes = QAssetHeaderSize + storedPayload.size();
        return true;
    }
}