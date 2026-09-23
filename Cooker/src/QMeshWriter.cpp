#include "QMeshWriter.h"

#include <bit>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>
#include <mutex>

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

        constexpr std::uint64_t QMeshHeaderSize =
            // Vertex format, stride, counts and bounds-valid.
            sizeof(std::uint32_t) * 6u +
            // Bounds min/max.
            sizeof(float) * 6u +
            // Encoded stream sizes.
            sizeof(std::uint64_t) * 2u;

        constexpr std::uint64_t QMeshSubmeshSize = sizeof(std::uint32_t) * 5u;

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

        std::vector<Assets::QMeshVertexV1>
        BuildDiskVertices(const Assets::MeshImportResult& mesh)
        {
            std::vector<Assets::QMeshVertexV1> vertices;
            vertices.reserve(mesh.Vertices.size());

            for (const Assets::MeshVertex& source : mesh.Vertices)
            {
                Assets::QMeshVertexV1 vertex{};

                vertex.Position[0] = source.Position.x;
                vertex.Position[1] = source.Position.y;
                vertex.Position[2] = source.Position.z;

                vertex.Color[0] = source.Color.x;
                vertex.Color[1] = source.Color.y;
                vertex.Color[2] = source.Color.z;

                vertex.Normal[0] = source.Normal.x;
                vertex.Normal[1] = source.Normal.y;
                vertex.Normal[2] = source.Normal.z;

                vertex.TexCoord[0] = source.TexCoord.x;
                vertex.TexCoord[1] = source.TexCoord.y;

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

        if (mesh.Vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
            mesh.Indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            mesh.Submeshes.size() > std::numeric_limits<std::uint32_t>::max())
        {
            outError = "Mesh exceeds QMesh v1 count limits.";
            return false;
        }

        const std::vector<Assets::QMeshVertexV1> diskVertices = BuildDiskVertices(mesh);
        std::vector<unsigned int> diskIndices;
        diskIndices.reserve(mesh.Indices.size());

        for (const Assets::MeshIndex index : mesh.Indices)
        {
            diskIndices.push_back(static_cast<unsigned int>(index));
        }

        std::vector<unsigned char> encodedVertices(
            meshopt_encodeVertexBufferBound(
                diskVertices.size(),
                sizeof(Assets::QMeshVertexV1)));

        const std::size_t encodedVertexSize =
            meshopt_encodeVertexBuffer(
                encodedVertices.data(),
                encodedVertices.size(),
                diskVertices.data(),
                diskVertices.size(),
                sizeof(Assets::QMeshVertexV1));

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

        const std::uint64_t submeshBytes = mesh.Submeshes.size() * QMeshSubmeshSize;
        const std::uint64_t payloadSize = QMeshHeaderSize + submeshBytes + encodedVertices.size() + encodedIndices.size();

        std::error_code ec;

        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                outError = "Failed to create QMesh output directory: " + ec.message();
                return false;
            }
        }

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
        const std::uint32_t flags = Assets::QAssetFlagNone;

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
        const std::uint32_t vertexFormat = static_cast<std::uint32_t>(Assets::QMeshVertexFormat::Float32PositionColorNormalTexCoord);
        const std::uint32_t vertexStride = sizeof(Assets::QMeshVertexV1);
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(diskVertices.size());
        const std::uint32_t indexCount = static_cast<std::uint32_t>(diskIndices.size());
        const std::uint32_t submeshCount = static_cast<std::uint32_t>(mesh.Submeshes.size());
        const std::uint32_t boundsValid = mesh.Bounds.Valid ? 1u : 0u;

        if (!WriteValue(output, vertexFormat) ||
            !WriteValue(output, vertexStride) ||
            !WriteValue(output, vertexCount) ||
            !WriteValue(output, indexCount) ||
            !WriteValue(output, submeshCount) ||
            !WriteValue(output, boundsValid))
        {
            outError = "Failed to write QMesh geometry header.";
            return false;
        }

        const float bounds[6] =
        {
            mesh.Bounds.Valid ? mesh.Bounds.Min.x : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Min.y : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Min.z : 0.0f,

            mesh.Bounds.Valid ? mesh.Bounds.Max.x : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Max.y : 0.0f,
            mesh.Bounds.Valid ? mesh.Bounds.Max.z : 0.0f
        };

        if (!WriteBytes(output, bounds, sizeof(bounds)))
        {
            outError = "Failed to write QMesh bounds.";
            return false;
        }

        const std::uint64_t vertexStreamSize = encodedVertices.size();
        const std::uint64_t indexStreamSize = encodedIndices.size();

        if (!WriteValue(output, vertexStreamSize) || !WriteValue(output, indexStreamSize))
        {
            outError = "Failed to write QMesh stream sizes.";
            return false;
        }

        // Submeshes
        for (const Assets::ImportedSubmesh& submesh : mesh.Submeshes)
        {
            const Assets::QMeshSubmeshV1 diskSubmesh
            {
                submesh.BaseVertex,
                submesh.VertexCount,
                submesh.BaseIndex,
                submesh.IndexCount,
                submesh.MaterialIndex
            };

            if (!WriteValue(output, diskSubmesh.BaseVertex) ||
                !WriteValue(output, diskSubmesh.VertexCount) ||
                !WriteValue(output, diskSubmesh.BaseIndex) ||
                !WriteValue(output, diskSubmesh.IndexCount) ||
                !WriteValue(output, diskSubmesh.MaterialSlot))
            {
                outError = "Failed to write QMesh submesh table.";
                return false;
            }
        }

        // Encoded streams
        if (!WriteBytes(output, encodedVertices.data(), encodedVertices.size()) ||
            !WriteBytes(output, encodedIndices.data(), encodedIndices.size()))
        {
            outError = "Failed to write QMesh encoded streams.";
            return false;
        }

        output.flush();

        if (!output.good())
        {
            outError = "Failed to finalize QMesh file.";
            return false;
        }

        outStats.RawVertexBytes = diskVertices.size() * sizeof(Assets::QMeshVertexV1);
        outStats.EncodedVertexBytes = encodedVertices.size();
        outStats.RawIndexBytes = diskIndices.size() * sizeof(unsigned int);
        outStats.EncodedIndexBytes = encodedIndices.size();
        outStats.FileBytes = QAssetHeaderSize + payloadSize;
        return true;
    }
}