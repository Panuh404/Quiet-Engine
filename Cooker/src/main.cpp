#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include <cmath>
#include <cstring>
#include <memory>
#include <chrono>

#include <meshoptimizer.h>
#include <ktx.h>
#include <cmp_core.h>
#include <vulkan/vulkan_core.h>

#include "QMeshWriter.h"
#include "QTextureWriter.h"
#include "QTextureMipGenerator.h"
#include "QTextureCompressor.h"

#include "Quiet/Assets/MeshImporter.h"
#include "Quiet/Assets/QMeshReader.h"
#include "Quiet/Assets/QTextureReader.h"
#include "Quiet/Assets/TextureImporter.h"
#include "Quiet/Assets/TexturePixelData.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectDescriptor.h"
#include "Quiet/Project/RuntimeManifest.h"

static_assert(MESHOPTIMIZER_VERSION == 1020, "Quiet_Cooker expects meshoptimizer v1.2.");
static_assert(KTX_SUCCESS == 0, "Quiet_Cooker expects the libktx API.");

namespace
{
    struct AssetCounts
    {
        std::size_t Meshes = 0;
        std::size_t Textures = 0;
        std::size_t Audio = 0;
        std::size_t Scripts = 0;
    };

    struct MeshOptimizationStats
    {
        std::size_t SourceVertices = 0;
        std::size_t OptimizedVertices = 0;

        std::size_t Indices = 0;
        std::size_t Triangles = 0;
        std::size_t Submeshes = 0;

        std::uint64_t SourceTransformedVertices = 0;
        std::uint64_t OptimizedTransformedVertices = 0;

        std::uint64_t SourceBytesFetched = 0;
        std::uint64_t OptimizedBytesFetched = 0;

        std::uint64_t SourceVertexBytes = 0;
        std::uint64_t OptimizedVertexBytes = 0;

        std::uint64_t SourcePixelsCovered = 0;
        std::uint64_t SourcePixelsShaded = 0;

        std::uint64_t OptimizedPixelsCovered = 0;
        std::uint64_t OptimizedPixelsShaded = 0;

        [[nodiscard]] double SourceACMR() const
        {
            return Triangles > 0
                ? static_cast<double>(SourceTransformedVertices) /
                  static_cast<double>(Triangles)
                : 0.0;
        }

        [[nodiscard]] double OptimizedACMR() const
        {
            return Triangles > 0
                ? static_cast<double>(OptimizedTransformedVertices) /
                  static_cast<double>(Triangles)
                : 0.0;
        }

        [[nodiscard]] double SourceOverfetch() const
        {
            return SourceVertexBytes > 0
                ? static_cast<double>(SourceBytesFetched) /
                  static_cast<double>(SourceVertexBytes)
                : 0.0;
        }

        [[nodiscard]] double OptimizedOverfetch() const
        {
            return OptimizedVertexBytes > 0
                ? static_cast<double>(OptimizedBytesFetched) /
                  static_cast<double>(OptimizedVertexBytes)
                : 0.0;
        }

        [[nodiscard]] double SourceOverdraw() const
        {
            return SourcePixelsCovered > 0
                ? static_cast<double>(SourcePixelsShaded) /
                  static_cast<double>(SourcePixelsCovered)
                : 0.0;
        }

        [[nodiscard]] double OptimizedOverdraw() const
        {
            return OptimizedPixelsCovered > 0
                ? static_cast<double>(OptimizedPixelsShaded) /
                  static_cast<double>(OptimizedPixelsCovered)
                : 0.0;
        }
    };

    struct QMeshQuantizationStats
    {
        float MaxPositionError = 0.0f;
        float MaxNormalAngleDegrees = 0.0f;
        float MaxColorError = 0.0f;
        float MaxTexCoordError = 0.0f;
    };

    AssetCounts CountProjectAssets(const Quiet::Project::AssetRegistry& registry)
    {
        AssetCounts counts{};

        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            // Engine-provided assets do not belong in the project package.
            if (record.BuiltIn) continue;

            switch (record.Type)
            {
                case Quiet::Project::AssetType::Mesh:
                    ++counts.Meshes;
                    break;

                case Quiet::Project::AssetType::Texture:
                    ++counts.Textures;
                    break;

                case Quiet::Project::AssetType::Audio:
                    ++counts.Audio;
                    break;

                case Quiet::Project::AssetType::Script:
                    ++counts.Scripts;
                    break;

                case Quiet::Project::AssetType::Unknown:
                default:
                    break;
            }
        }

        return counts;
    }

    std::filesystem::path ResolveAssetPath(const std::filesystem::path& projectRoot, const Quiet::Project::AssetRecord& record)
    {
        if (record.Path.empty()) return {};
        const std::filesystem::path rawPath = record.Path;

        return rawPath.is_absolute() ? rawPath.lexically_normal() : (projectRoot / rawPath).lexically_normal();
    }

    bool PrepareOutputLayout(const std::filesystem::path& outputRoot, std::string& outError)
    {
        static constexpr std::array<const char*, 3> directories =
        {
            "Scenes",
            "Content/Meshes",
            "Content/Textures"
        };

        std::error_code ec;

        for (const char* directory : directories)
        {
            std::filesystem::create_directories(outputRoot / directory, ec);
            if (ec)
            {
                outError = "Failed to create output directory '" + (outputRoot / directory).string() + "': " + ec.message();
                return false;
            }
        }

        return true;
    }

    void PrintUsage(const char* executable)
    {
        std::cerr << "Usage:\n" << "  " << executable << " <project.quiet> <output-directory>\n";
    }

    bool SameTriangleRotation(
        const Quiet::Assets::MeshIndex a0,
        const Quiet::Assets::MeshIndex a1,
        const Quiet::Assets::MeshIndex a2,
        const Quiet::Assets::MeshIndex b0,
        const Quiet::Assets::MeshIndex b1,
        const Quiet::Assets::MeshIndex b2)
    {
        return
            (a0 == b0 && a1 == b1 && a2 == b2) ||
            (a0 == b1 && a1 == b2 && a2 == b0) ||
            (a0 == b2 && a1 == b0 && a2 == b1);
    }

    bool ValidateQMeshRoundTrip(const std::filesystem::path& path,
                                const Quiet::Assets::MeshImportResult& source,
                                QMeshQuantizationStats& outStats,
                                std::string& outError)
    {
        outStats = {};
        Quiet::Assets::QMeshData decoded{};
        if (!Quiet::Assets::QMeshReader::LoadFromFile(path, decoded, outError)) return false;
        if (decoded.Vertices.size() != source.Vertices.size() ||
            decoded.Indices.size() != source.Indices.size() ||
            decoded.Submeshes.size() != source.Submeshes.size())
        {
            outError = "QMesh round-trip changed geometry counts.";
            return false;
        }

        for (std::size_t i = 0; i < source.Vertices.size(); ++i)
        {
            const auto& a = source.Vertices[i];
            const auto& b = decoded.Vertices[i];

            const float dx = a.Position.x - b.Position.x;
            const float dy = a.Position.y - b.Position.y;
            const float dz = a.Position.z - b.Position.z;

            const float positionError = std::sqrt(dx * dx + dy * dy + dz * dz);
            outStats.MaxPositionError = std::max(outStats.MaxPositionError, positionError);

            const auto NormalizedDot = [](const glm::vec3& lhs, const glm::vec3& rhs)
            {
                const float lhsLength = std::sqrt(lhs.x * lhs.x + lhs.y * lhs.y + lhs.z * lhs.z);
                const float rhsLength = std::sqrt(rhs.x * rhs.x + rhs.y * rhs.y + rhs.z * rhs.z);

                if (lhsLength <= 1.0e-20f || rhsLength <= 1.0e-20f) return 1.0f;
                return std::clamp((lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z) / (lhsLength * rhsLength), -1.0f, 1.0f);
            };

            constexpr float RadToDeg = 57.29577951308232f;
            const float normalAngle = std::acos(NormalizedDot(a.Normal, b.Normal)) * RadToDeg;
            outStats.MaxNormalAngleDegrees = std::max(outStats.MaxNormalAngleDegrees, normalAngle);
            outStats.MaxColorError = std::max(
                outStats.MaxColorError,
                std::max({
                    std::abs(a.Color.x - b.Color.x),
                    std::abs(a.Color.y - b.Color.y),
                    std::abs(a.Color.z - b.Color.z)
                }));
            outStats.MaxTexCoordError = std::max(
                outStats.MaxTexCoordError,
                std::max(
                    std::abs(a.TexCoord.x - b.TexCoord.x),
                    std::abs(a.TexCoord.y - b.TexCoord.y)));
        }

        const glm::vec3 extent = source.Bounds.Max - source.Bounds.Min;
        const float positionStepX = extent.x / 65535.0f;
        const float positionStepY = extent.y / 65535.0f;
        const float positionStepZ = extent.z / 65535.0f;

        const float maximumExpectedPositionError = std::sqrt(positionStepX * positionStepX + positionStepY * positionStepY + positionStepZ * positionStepZ) + 1.0e-6f;

        if (outStats.MaxPositionError > maximumExpectedPositionError)
        {
            outError = "QMesh position quantization exceeded expected error.";
            return false;
        }

        if (outStats.MaxColorError > (1.0f / 255.0f) + 1.0e-6f)
        {
            outError = "QMesh color quantization exceeded expected error.";
            return false;
        }

        if (outStats.MaxNormalAngleDegrees > 0.1f)
        {
            outError = "QMesh normal quantization exceeded expected angular error.";
            return false;
        }

        for (std::size_t i = 0; i < source.Indices.size(); i += 3)
        {
            if (!SameTriangleRotation(
                    source.Indices[i + 0],
                    source.Indices[i + 1],
                    source.Indices[i + 2],
                    decoded.Indices[i + 0],
                    decoded.Indices[i + 1],
                    decoded.Indices[i + 2]))
            {
                outError = "QMesh round-trip changed triangle topology.";
                return false;
            }
        }

        for (std::size_t i = 0; i < source.Submeshes.size(); ++i)
        {
            const auto& sourceSubmesh = source.Submeshes[i];
            const auto& decodedSubmesh = decoded.Submeshes[i];

            if (sourceSubmesh.BaseVertex != decodedSubmesh.BaseVertex ||
                sourceSubmesh.VertexCount != decodedSubmesh.VertexCount ||
                sourceSubmesh.BaseIndex != decodedSubmesh.BaseIndex ||
                sourceSubmesh.IndexCount != decodedSubmesh.IndexCount ||
                sourceSubmesh.MaterialIndex != decodedSubmesh.MaterialSlot)
            {
                outError = "QMesh round-trip changed submesh metadata.";
                return false;
            }
        }

        if (source.Bounds.Valid != decoded.Bounds.Valid)
        {
            outError = "QMesh round-trip changed bounds validity.";
            return false;
        }

        if (source.Bounds.Valid)
        {
            if (source.Bounds.Min.x != decoded.Bounds.Min.x ||
                source.Bounds.Min.y != decoded.Bounds.Min.y ||
                source.Bounds.Min.z != decoded.Bounds.Min.z ||
                source.Bounds.Max.x != decoded.Bounds.Max.x ||
                source.Bounds.Max.y != decoded.Bounds.Max.y ||
                source.Bounds.Max.z != decoded.Bounds.Max.z)
            {
                outError = "QMesh round-trip changed mesh bounds.";
                return false;
            }
        }

        return true;
    }

    void AccumulateMeshEfficiency(const std::vector<Quiet::Assets::MeshVertex>& vertices, const std::vector<unsigned int>& indices, MeshOptimizationStats& stats, const bool optimized)
    {
        if (vertices.empty() || indices.empty()) return;

        const meshopt_VertexCacheStatistics cacheStats =
            meshopt_analyzeVertexCache(
                indices.data(),
                indices.size(),
                vertices.size(),
                16,
                0,
                0
            );

        const meshopt_VertexFetchStatistics fetchStats =
            meshopt_analyzeVertexFetch(
                indices.data(),
                indices.size(),
                vertices.size(),
                sizeof(Quiet::Assets::MeshVertex)
            );

        const meshopt_OverdrawStatistics overdrawStats =
            meshopt_analyzeOverdraw(
                indices.data(),
                indices.size(),
                &vertices.front().Position.x,
                vertices.size(),
                sizeof(Quiet::Assets::MeshVertex)
            );

        const std::uint64_t vertexBytes = vertices.size() * sizeof(Quiet::Assets::MeshVertex);

        if (optimized)
        {
            stats.OptimizedTransformedVertices += cacheStats.vertices_transformed;
            stats.OptimizedBytesFetched += fetchStats.bytes_fetched;
            stats.OptimizedVertexBytes += vertexBytes;
            stats.OptimizedPixelsCovered += overdrawStats.pixels_covered;
            stats.OptimizedPixelsShaded += overdrawStats.pixels_shaded;
        }
        else
        {
            stats.SourceTransformedVertices += cacheStats.vertices_transformed;
            stats.SourceBytesFetched += fetchStats.bytes_fetched;
            stats.SourceVertexBytes += vertexBytes;
            stats.SourcePixelsCovered += overdrawStats.pixels_covered;
            stats.SourcePixelsShaded += overdrawStats.pixels_shaded;
        }
    }

    bool OptimizeMeshForCooking(Quiet::Assets::MeshImportResult& mesh, MeshOptimizationStats& outStats, std::string& outError)
    {
        static_assert(std::is_standard_layout_v<Quiet::Assets::MeshVertex>, "MeshVertex must remain standard-layout for meshoptimizer.");
        static_assert(offsetof(Quiet::Assets::MeshVertex, Position) == 0, "MeshVertex position must remain the first vertex attribute.");

        if (!mesh.Succeeded())
        {
            outError = "Cannot optimize an unsuccessful mesh import.";
            return false;
        }

        if (mesh.Submeshes.empty())
        {
            outError = "Imported mesh contains no submeshes.";
            return false;
        }

        outStats = {};
        outStats.SourceVertices = mesh.Vertices.size();
        outStats.Indices = mesh.Indices.size();
        outStats.Triangles = mesh.Indices.size() / 3;
        outStats.Submeshes = mesh.Submeshes.size();

        std::vector<Quiet::Assets::MeshVertex> optimizedVertices;
        std::vector<Quiet::Assets::MeshIndex> optimizedIndices;
        std::vector<Quiet::Assets::ImportedSubmesh> optimizedSubmeshes;

        optimizedVertices.reserve(mesh.Vertices.size());
        optimizedIndices.reserve(mesh.Indices.size());
        optimizedSubmeshes.reserve(mesh.Submeshes.size());

        for (const Quiet::Assets::ImportedSubmesh& sourceSubmesh : mesh.Submeshes)
        {
            const std::size_t sourceVertexBegin = sourceSubmesh.BaseVertex;
            const std::size_t sourceVertexEnd = sourceVertexBegin + sourceSubmesh.VertexCount;
            const std::size_t sourceIndexBegin = sourceSubmesh.BaseIndex;
            const std::size_t sourceIndexEnd = sourceIndexBegin + sourceSubmesh.IndexCount;

            if (sourceVertexEnd > mesh.Vertices.size())
            {
                outError = "Submesh '" + sourceSubmesh.Name + "' has an invalid vertex range.";
                return false;
            }

            if (sourceIndexEnd > mesh.Indices.size())
            {
                outError = "Submesh '" + sourceSubmesh.Name + "' has an invalid index range.";
                return false;
            }

            if (sourceSubmesh.IndexCount % 3 != 0)
            {
                outError = "Submesh '" + sourceSubmesh.Name + "' does not contain triangle-list indices.";
                return false;
            }

            Quiet::Assets::ImportedSubmesh optimizedSubmesh = sourceSubmesh;
            optimizedSubmesh.BaseVertex = static_cast<std::uint32_t>(optimizedVertices.size());
            optimizedSubmesh.BaseIndex = static_cast<std::uint32_t>(optimizedIndices.size());

            if (sourceSubmesh.VertexCount == 0 || sourceSubmesh.IndexCount == 0)
            {
                optimizedSubmesh.VertexCount = 0;
                optimizedSubmesh.IndexCount = 0;
                optimizedSubmeshes.push_back(std::move(optimizedSubmesh));
                continue;
            }

            std::vector localVertices(
                mesh.Vertices.begin() + static_cast<std::ptrdiff_t>(sourceVertexBegin),
                mesh.Vertices.begin() + static_cast<std::ptrdiff_t>(sourceVertexEnd));

            std::vector<unsigned int> localIndices;
            localIndices.reserve(sourceSubmesh.IndexCount);

            for (std::size_t indexOffset = sourceIndexBegin; indexOffset < sourceIndexEnd; ++indexOffset)
            {
                const Quiet::Assets::MeshIndex globalIndex = mesh.Indices[indexOffset];
                if (globalIndex < sourceVertexBegin || globalIndex >= sourceVertexEnd)
                {
                    outError = "Submesh '" + sourceSubmesh.Name + "' references a vertex outside its vertex range.";
                    return false;
                }

                localIndices.push_back(static_cast<unsigned int>(globalIndex - sourceVertexBegin));
            }

            AccumulateMeshEfficiency(localVertices, localIndices, outStats, false);

            meshopt_optimizeVertexCache(
                localIndices.data(),
                localIndices.data(),
                localIndices.size(),
                localVertices.size()
            );

            meshopt_optimizeOverdraw(
                localIndices.data(),
                localIndices.data(),
                localIndices.size(),
                &localVertices.front().Position.x,
                localVertices.size(),
                sizeof(Quiet::Assets::MeshVertex),
                1.05f
            );

            std::vector<Quiet::Assets::MeshVertex> fetchedVertices(localVertices.size());

            const std::size_t optimizedVertexCount =
                meshopt_optimizeVertexFetch(
                    fetchedVertices.data(),
                    localIndices.data(),
                    localIndices.size(),
                    localVertices.data(),
                    localVertices.size(),
                    sizeof(Quiet::Assets::MeshVertex)
                );

            fetchedVertices.resize(optimizedVertexCount);

            AccumulateMeshEfficiency(
                fetchedVertices,
                localIndices,
                outStats,
                true
            );

            const std::size_t outputBaseVertex = optimizedVertices.size();
            optimizedVertices.insert(optimizedVertices.end(), fetchedVertices.begin(), fetchedVertices.end());

            for (const unsigned int localIndex : localIndices)
            {
                const std::size_t globalIndex = outputBaseVertex + localIndex;
                if (globalIndex > std::numeric_limits<Quiet::Assets::MeshIndex>::max())
                {
                    outError = "Optimized mesh exceeds uint32 index range.";
                    return false;
                }

                optimizedIndices.push_back(static_cast<Quiet::Assets::MeshIndex>(globalIndex));
            }

            optimizedSubmesh.VertexCount = static_cast<std::uint32_t>(fetchedVertices.size());
            optimizedSubmesh.IndexCount = static_cast<std::uint32_t>(localIndices.size());
            optimizedSubmeshes.push_back(std::move(optimizedSubmesh));
        }

        mesh.Vertices = std::move(optimizedVertices);
        mesh.Indices = std::move(optimizedIndices);
        mesh.Submeshes = std::move(optimizedSubmeshes);

        outStats.OptimizedVertices = mesh.Vertices.size();
        return true;
    }

    bool OptimizeProjectMeshes(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& outputRoot,
        const Quiet::Project::AssetRegistry& registry,
        std::vector<Quiet::Project::RuntimeAssetRecord>& outRuntimeAssets)
    {
        bool success = true;

        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            if (record.BuiltIn || record.Type != Quiet::Project::AssetType::Mesh) continue;

            const std::filesystem::path sourcePath = ResolveAssetPath(projectRoot, record);
            if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath))
            {
                std::cerr << "Mesh '" << record.ID << "' points to a missing source file: " << sourcePath << '\n';
                success = false;
                continue;
            }

            Quiet::Assets::MeshImportResult mesh = Quiet::Assets::MeshImporter::LoadFromFile(sourcePath);
            if (!mesh.Succeeded())
            {
                std::cerr << "Failed to import mesh '" << record.ID << "': " << mesh.Error << '\n';
                success = false;
                continue;
            }

            MeshOptimizationStats stats{};
            std::string optimizationError;

            if (!OptimizeMeshForCooking(mesh, stats, optimizationError))
            {
                std::cerr << "Failed to optimize mesh '" << record.ID << "': " << optimizationError << '\n';
                success = false;
                continue;
            }

            const std::filesystem::path runtimePath = std::filesystem::path("Content") / "Meshes" / (record.ID + ".qmesh");
            const std::filesystem::path cookedPath = outputRoot / runtimePath;
            Quiet::Cooker::QMeshWriteStats writeStats{};
            std::string writeError;

            if (!Quiet::Cooker::WriteQMesh(cookedPath, mesh, writeStats, writeError))
            {
                std::cerr << "Failed to write mesh '" << record.ID << "': " << writeError << '\n';
                success = false;
                continue;
            }

            QMeshQuantizationStats quantizationStats{};
            std::string validationError;

            if (!ValidateQMeshRoundTrip(cookedPath, mesh, quantizationStats, validationError))
            {
                std::cerr << "Failed to validate mesh '" << record.ID << "': " << validationError << '\n';
                success = false;
                continue;
            }

            Quiet::Project::RuntimeAssetRecord runtimeRecord{};
            runtimeRecord.ID = record.ID;
            runtimeRecord.Type = Quiet::Project::AssetType::Mesh;
            runtimeRecord.Path = runtimePath.generic_string();
            outRuntimeAssets.push_back(std::move(runtimeRecord));

            std::cout
                << "Mesh: " << record.ID << '\n'
                << "  Vertices:  " << stats.SourceVertices << " -> " << stats.OptimizedVertices << '\n'
                << "  Triangles: " << stats.Triangles << '\n'
                << std::fixed
                << std::setprecision(3)
                << "  ACMR:      " << stats.SourceACMR() << " -> " << stats.OptimizedACMR() << '\n'
                << "  Overfetch: " << stats.SourceOverfetch() << " -> " << stats.OptimizedOverfetch() << '\n'
                << "  Overdraw:  " << stats.SourceOverdraw() << " -> " << stats.OptimizedOverdraw() << '\n';


            const std::uint64_t rawGeometryBytes = writeStats.RawVertexBytes + writeStats.RawIndexBytes;
            const std::uint64_t encodedGeometryBytes = writeStats.EncodedVertexBytes + writeStats.EncodedIndexBytes;
            const double encodedPercent = rawGeometryBytes > 0
                ? static_cast<double>(encodedGeometryBytes) / static_cast<double>(rawGeometryBytes) * 100.0
                : 0.0;

            const auto percentage = [](const std::uint64_t encoded, const std::uint64_t raw)
            {
                return raw > 0 ? static_cast<double>(encoded) / static_cast<double>(raw) * 100.0 : 0.0;
            };

            const std::uint64_t packedGeometryBytes = writeStats.PackedVertexBytes + writeStats.RawIndexBytes;
            const double packedGeometryPercent = percentage(packedGeometryBytes, rawGeometryBytes);
            const double payloadPercent = writeStats.UncompressedPayloadBytes > 0
                ? static_cast<double>(writeStats.StoredPayloadBytes) / static_cast<double>(writeStats.UncompressedPayloadBytes) * 100.0
                : 0.0;

            std::cout
            << "  QMesh:      " << cookedPath.filename().string() << '\n'
            << "  Vertex raw:     " << writeStats.RawVertexBytes << " bytes\n" << "  Vertex packed:  " << writeStats.PackedVertexBytes << " bytes (" << percentage(writeStats.PackedVertexBytes, writeStats.RawVertexBytes) << "% of raw)\n"
            << "  Vertex encoded: " << writeStats.EncodedVertexBytes << " bytes (" << percentage(writeStats.EncodedVertexBytes, writeStats.PackedVertexBytes) << "% of packed)\n"
            << "  Index data:     " << writeStats.RawIndexBytes << " -> " << writeStats.EncodedIndexBytes << " bytes (" << percentage(writeStats.EncodedIndexBytes, writeStats.RawIndexBytes) << "%)\n"
            << "  Geometry raw:   " << rawGeometryBytes << " bytes\n"
            << "  Geometry packed:" << ' ' << packedGeometryBytes << " bytes (" << packedGeometryPercent << "%)\n"
            << "  Geometry encoded:" << ' ' << encodedGeometryBytes << " bytes\n"
            << "  Payload:        " << writeStats.UncompressedPayloadBytes << " -> " << writeStats.StoredPayloadBytes << " bytes (" << payloadPercent << "%)\n"
            << "  Zstandard:      " << (writeStats.ZstdCompressed ? "enabled" : "skipped (no size benefit)") << '\n'
            << "  File size:      " << writeStats.FileBytes << " bytes\n"
            << "  Round-trip:     OK\n"

            << "  Quantization:\n"
            << "    Position max error: " << quantizationStats.MaxPositionError << '\n'
            << "    Normal max error:   " << quantizationStats.MaxNormalAngleDegrees << " deg\n"
            << "    Color max error:    " << quantizationStats.MaxColorError << '\n'
            << "    UV max error:       " << quantizationStats.MaxTexCoordError << "\n\n";
        }
        return success;
    }

    struct KtxTexture2Deleter
    {
        void operator()(ktxTexture2* texture) const noexcept
        {
            if (texture) ktxTexture2_Destroy(texture);
        }
    };

    using KtxTexture2Ptr = std::unique_ptr<ktxTexture2, KtxTexture2Deleter>;

    bool ValidateQTextureRoundTrip(
        const std::filesystem::path& path,
        const std::vector<Quiet::Cooker::QTextureBC7MipLevel>& expectedMipLevels,
        std::string& outError)
    {
        outError.clear();

        if (expectedMipLevels.empty())
        {
            outError = "Cannot validate an empty QTexture mip chain.";
            return false;
        }

        for (const Quiet::Cooker::QTextureBC7MipLevel& mip : expectedMipLevels)
        {
            if (!mip.IsValid())
            {
                outError = "Cannot validate an invalid BC7 mip chain.";
                return false;
            }
        }

        Quiet::Assets::QTextureData cookedTexture{};
        if (!Quiet::Assets::QTextureReader::LoadFromFile(path, cookedTexture, outError))
        {
            return false;
        }

        if (cookedTexture.Format != Quiet::Assets::QTexturePayloadFormat::Ktx2)
        {
            outError = "QTexture reader returned an unexpected payload format.";
            return false;
        }

        if (cookedTexture.VulkanFormat != static_cast<std::uint32_t>(VK_FORMAT_BC7_SRGB_BLOCK))
        {
            outError = "QTexture reader returned an unexpected Vulkan format.";
            return false;
        }

        if (cookedTexture.Width != expectedMipLevels.front().Width ||
            cookedTexture.Height != expectedMipLevels.front().Height)
        {
            outError = "QTexture reader changed the texture dimensions.";
            return false;
        }

        if (cookedTexture.MipLevels.size() != expectedMipLevels.size())
        {
            outError = "QTexture reader returned an unexpected mip count.";
            return false;
        }

        for (std::size_t level = 0; level < expectedMipLevels.size(); ++level)
        {
            const Quiet::Cooker::QTextureBC7MipLevel& expected = expectedMipLevels[level];
            const Quiet::Assets::QTextureMipData& decoded = cookedTexture.MipLevels[level];

            if (decoded.Width != expected.Width || decoded.Height != expected.Height)
            {
                outError = "QTexture reader changed BC7 mip dimensions.";
                return false;
            }

            if (decoded.Data != expected.Blocks)
            {
                outError = "QTexture reader changed BC7 mip data.";
                return false;
            }
        }

        ktxTexture2* rawTexture = nullptr;

        const KTX_error_code result = ktxTexture2_CreateFromMemory(
            cookedTexture.EncodedData.data(),
            static_cast<ktx_size_t>(cookedTexture.EncodedData.size()),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &rawTexture);

        if (result != KTX_SUCCESS)
        {
            outError = std::string("Failed to parse embedded KTX2: ") + ktxErrorString(result);
            return false;
        }

        KtxTexture2Ptr texture(rawTexture);

        if (texture->vkFormat != VK_FORMAT_BC7_SRGB_BLOCK)
        {
            outError = "QTexture KTX2 has an unexpected Vulkan format.";
            return false;
        }

        if (texture->baseWidth != expectedMipLevels.front().Width ||
            texture->baseHeight != expectedMipLevels.front().Height)
        {
            outError = "QTexture KTX2 dimensions changed during round-trip.";
            return false;
        }

        if (texture->numDimensions != 2 ||
            texture->numLevels != expectedMipLevels.size() ||
            texture->numLayers != 1 ||
            texture->numFaces != 1)
        {
            outError = "QTexture KTX2 has an unexpected texture layout.";
            return false;
        }

        if (texture->supercompressionScheme != KTX_SS_NONE)
        {
            outError = "QTexture unexpectedly uses KTX2 supercompression.";
            return false;
        }

        ktxTexture* baseTexture = ktxTexture(texture.get());

        const ktx_uint8_t* imageData = ktxTexture_GetData(baseTexture);
        const ktx_size_t dataSize = ktxTexture_GetDataSize(baseTexture);

        if (!imageData)
        {
            outError = "QTexture KTX2 contains no loaded image data.";
            return false;
        }

        for (std::size_t level = 0; level < expectedMipLevels.size(); ++level)
        {
            const Quiet::Cooker::QTextureBC7MipLevel& expected = expectedMipLevels[level];
            const ktx_size_t imageSize = ktxTexture_GetImageSize(baseTexture, static_cast<ktx_uint32_t>(level));

            if (imageSize != expected.Blocks.size())
            {
                outError = "QTexture BC7 mip byte size changed during round-trip.";
                return false;
            }

            ktx_size_t imageOffset = 0;

            const KTX_error_code offsetResult = ktxTexture_GetImageOffset(baseTexture, static_cast<ktx_uint32_t>(level), 0, 0, &imageOffset);
            if (offsetResult != KTX_SUCCESS)
            {
                outError = std::string("Failed to resolve KTX2 mip offset: ") + ktxErrorString(offsetResult);
                return false;
            }

            if (imageOffset > dataSize || expected.Blocks.size() > dataSize - imageOffset)
            {
                outError = "QTexture BC7 mip range exceeds loaded KTX2 data.";
                return false;
            }

            if (std::memcmp(imageData + imageOffset, expected.Blocks.data(), expected.Blocks.size()) != 0)
            {
                outError = "QTexture BC7 mip data changed during round-trip.";
                return false;
            }
        }

        return true;
    }

    bool CookProjectTextures(const std::filesystem::path& projectRoot, const std::filesystem::path& outputRoot, const Quiet::Project::AssetRegistry& registry)
    {
        bool success = true;

        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            if (record.BuiltIn || record.Type != Quiet::Project::AssetType::Texture) continue;

            const std::filesystem::path sourcePath = ResolveAssetPath(projectRoot, record);
            if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath))
            {
                std::cerr << "Texture '" << record.ID << "' points to a missing source file: " << sourcePath << '\n';
                success = false;
                continue;
            }

            Quiet::Assets::TexturePixelData pixels = Quiet::Assets::TextureImporter::LoadPixelsFromFile(sourcePath);
            if (!pixels.IsValid())
            {
                std::cerr << "Failed to import texture '" << record.ID << "': " << pixels.Metadata.Error << '\n';
                success = false;
                continue;
            }

            std::vector<Quiet::Cooker::QTextureMipLevel> mipLevels;
            std::string mipError;

            if (!Quiet::Cooker::BuildRgba8SrgbMipChain(pixels, mipLevels, mipError))
            {
                std::cerr << "Failed to generate mipmaps for texture '" << record.ID << "': " << mipError << '\n';
                success = false;
                continue;
            }

            std::vector<Quiet::Cooker::QTextureBC7MipLevel> bc7MipLevels;
            Quiet::Cooker::QTextureBC7CompressionStats bc7Stats{};

            std::string compressionError;
            constexpr float BC7IntegrationQuality = 0.05f;
            const auto bc7Start = std::chrono::steady_clock::now();

            if (!Quiet::Cooker::CompressRgba8MipChainToBC7(mipLevels, BC7IntegrationQuality, bc7MipLevels, bc7Stats, compressionError))
            {
                std::cerr << "Failed to BC7-compress texture '" << record.ID << "': " << compressionError << '\n';
                success = false;
                continue;
            }

            const auto bc7End = std::chrono::steady_clock::now();
            const double bc7Seconds = std::chrono::duration<double>(bc7End - bc7Start).count();

            const std::filesystem::path cookedPath = outputRoot / "Content" / "Textures" / (record.ID + ".qtexture");
            Quiet::Cooker::QTextureWriteStats stats{};
            std::string writeError;

            if (!Quiet::Cooker::WriteQTexture(cookedPath, bc7MipLevels, stats, writeError))
            {
                std::cerr << "Failed to write texture '" << record.ID << "': " << writeError << '\n';
                success = false;
                continue;
            }

            std::string validationError;

            if (!ValidateQTextureRoundTrip(cookedPath, bc7MipLevels, validationError))
            {
                std::cerr << "Failed to validate texture '" << record.ID << "': " << validationError << '\n';
                success = false;
                continue;
            }

            const auto percentage = [](const std::uint64_t value, const std::uint64_t reference)
            {
                return reference > 0 ? static_cast<double>(value) / static_cast<double>(reference) * 100.0 : 0.0;
            };

            const std::uint64_t baseRgbaBytes =
                static_cast<std::uint64_t>(mipLevels.front().Pixels.size());

            std::cout
            << "Texture: " << record.ID << '\n'
            << "  Source:      " << sourcePath.filename().string() << '\n'
            << "  Format:      " << pixels.Metadata.Format << " -> QTexture/KTX2 BC7 sRGB\n"
            << "  Dimensions:  " << stats.Width << 'x' << stats.Height << '\n'
            << "  Mip levels:  " << stats.MipLevels << '\n'
            << "  RGBA8 data:  " << baseRgbaBytes << " bytes\n"
            << "  Mip data:    " << bc7Stats.SourceBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(bc7Stats.SourceBytes, baseRgbaBytes) << "% of base)\n"
            << "  BC7 data:    " << bc7Stats.CompressedBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(bc7Stats.CompressedBytes, bc7Stats.SourceBytes) << "% of raw mip data)\n"
            << "  BC7 time:    " << std::fixed << std::setprecision(3) << bc7Seconds << " s\n"
            << "  KTX2 data:   " << stats.KtxBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(stats.KtxBytes, stats.BC7Bytes) << "% of BC7 data)\n"
            << "  QTexture:    " << cookedPath.filename().string() << '\n'
            << "  File size:   " << stats.FileBytes << " bytes\n"
            << "  GPU format:  BC7 sRGB (8 bpp)\n"
            << "  Outer comp.: none\n"
            << "  Round-trip:  OK\n\n";
        }

        return success;
    }

    struct CompressonatorBC7OptionsDeleter
    {
        void operator()(void* options) const noexcept
        {
            if (options) DestroyOptionsBC7(options);
        }
    };

    using CompressonatorBC7OptionsPtr = std::unique_ptr<void, CompressonatorBC7OptionsDeleter>;

    bool ValidateCompressonatorBC7(std::string& outError)
    {
        outError.clear();

        void* rawOptions = nullptr;
        if (CreateOptionsBC7(&rawOptions) != 0 || !rawOptions)
        {
            outError = "Failed to create Compressonator BC7 options.";
            return false;
        }

        CompressonatorBC7OptionsPtr options(rawOptions);
        if (SetQualityBC7(options.get(), 0.5f) != 0)
        {
            outError = "Failed to configure Compressonator BC7 quality.";
            return false;
        }

        // One 4x4 RGBA8 test block.
        //
        // BC7 always consumes 4x4 texels and produces
        // exactly 16 bytes.
        std::array<unsigned char, 64> source{};
        for (std::size_t pixel = 0; pixel < 16; ++pixel)
        {
            source[pixel * 4 + 0] = static_cast<unsigned char>(pixel * 13);
            source[pixel * 4 + 1] = static_cast<unsigned char>(255 - pixel * 11);
            source[pixel * 4 + 2] = static_cast<unsigned char>(pixel * 7);
            source[pixel * 4 + 3] = 255;
        }

        std::array<unsigned char, 16> compressed{};

        // Four RGBA8 pixels = 16 bytes per source row.
        if (CompressBlockBC7(source.data(), 16, compressed.data(), options.get()) != 0)
        {
            outError = "Compressonator BC7 block compression failed.";
            return false;
        }

        std::array<unsigned char, 64> decoded{};
        if (DecompressBlockBC7(compressed.data(), decoded.data(), options.get()) != 0)
        {
            outError = "Compressonator BC7 block decompression failed.";
            return false;
        }

        return true;
    }
}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    namespace Project = Quiet::Project;

    if (argc != 3)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string compressonatorError;

    if (!ValidateCompressonatorBC7(compressonatorError))
    {
        std::cerr << "Compressonator BC7 codec validation failed: " << compressonatorError << '\n';
        return 1;
    }

    std::cout << "Compressonator BC7 codec: OK\n\n";

    std::error_code ec;
    const fs::path projectFilePath = fs::absolute(argv[1], ec).lexically_normal();
    if (ec)
    {
        std::cerr << "Failed to resolve project path: " << ec.message() << '\n';
        return 1;
    }

    if (!fs::is_regular_file(projectFilePath, ec) || ec)
    {
        std::cerr << "Project file does not exist: " << projectFilePath << '\n';
        return 1;
    }

    const fs::path outputRoot = fs::absolute(argv[2], ec).lexically_normal();
    if (ec)
    {
        std::cerr << "Failed to resolve output path: " << ec.message() << '\n';
        return 1;
    }

    Project::ProjectDescriptor descriptor{};
    if (!Project::ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor))
    {
        std::cerr << "Failed to load project descriptor: " << projectFilePath << '\n';
        return 1;
    }

    const fs::path projectRoot = projectFilePath.parent_path();
    const fs::path assetRegistryPath = projectRoot / "AssetRegistry.json";

    Project::AssetRegistry assetRegistry{};
    if (!Project::AssetRegistryIO::LoadFromFile(assetRegistryPath, assetRegistry))
    {
        std::cerr << "Failed to load asset registry: " << assetRegistryPath << '\n';
        return 1;
    }

    const fs::path startupScenePath = projectRoot / descriptor.ScenesDirectory / descriptor.StartupScene;
    if (!fs::is_regular_file(startupScenePath, ec) || ec)
    {
        std::cerr << "Startup scene does not exist: " << startupScenePath << '\n';
        return 1;
    }

    std::string outputError;

    if (!PrepareOutputLayout(outputRoot, outputError))
    {
        std::cerr << outputError << '\n';
        return 1;
    }

    const AssetCounts counts = CountProjectAssets(assetRegistry);

    std::cout
        << "Quiet Asset Cooker\n"
        << "------------------\n"
        << "Project:      " << descriptor.Name << '\n'
        << "Source:       " << projectRoot << '\n'
        << "Output:       " << outputRoot << '\n'
        << "Startup Scene:" << ' ' << startupScenePath << '\n'
        << '\n'
        << "Cookable assets:\n"
        << "  Meshes:     " << counts.Meshes << '\n'
        << "  Textures:   " << counts.Textures << '\n'
        << "  Audio:      " << counts.Audio << " (deferred)\n"
        << "  Scripts:    " << counts.Scripts << " (deferred)\n"
        << '\n'
        << "Output layout prepared successfully.\n";

    std::cout << "\nOptimizing mesh assets...\n\n";

    std::vector<Project::RuntimeAssetRecord> runtimeAssets;
    if (!OptimizeProjectMeshes(projectRoot, outputRoot, assetRegistry, runtimeAssets))
    {
        std::cerr << "Mesh optimization completed with errors.\n";
        return 1;
    }

    std::cout << "\nCooking texture assets...\n\n";

    if (!CookProjectTextures(projectRoot, outputRoot, assetRegistry))
    {
        std::cerr << "Texture cooking completed with errors.\n";
        return 1;
    }

    std::cout << "Texture cooking completed successfully.\n";

    const fs::path runtimeScenePath = fs::path("Scenes") / fs::path(descriptor.StartupScene);
    const fs::path cookedScenePath = outputRoot / runtimeScenePath;

    fs::create_directories(cookedScenePath.parent_path(), ec);
    if (ec)
    {
        std::cerr << "Failed to prepare startup scene output directory: " << ec.message() << '\n';
        return 1;
    }

    fs::copy_file(startupScenePath, cookedScenePath, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        std::cerr << "Failed to copy startup scene: " << ec.message() << '\n';
        return 1;
    }

    Project::RuntimeManifest runtimeManifest{};
    runtimeManifest.ProjectName = descriptor.Name;
    runtimeManifest.StartupScene = runtimeScenePath.generic_string();
    runtimeManifest.Assets = std::move(runtimeAssets);

    const fs::path runtimeManifestPath = outputRoot / "RuntimeManifest.json";
    if (!Project::RuntimeManifestIO::SaveToFile(runtimeManifestPath, runtimeManifest))
    {
        std::cerr << "Failed to write runtime manifest: " << runtimeManifestPath << '\n';
        return 1;
    }

    std::cout
    << '\n'
    << "Runtime package metadata:\n"
    << "  Startup scene: " << runtimeManifest.StartupScene << '\n'
    << "  Runtime assets: " << runtimeManifest.Assets.size() << '\n'
    << "  Manifest:      " << runtimeManifestPath << '\n'
    << '\n'
    << "Asset cooking completed successfully.\n";
    return 0;
}