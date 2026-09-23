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

#include <meshoptimizer.h>
#include "QMeshWriter.h"

#include "Quiet/Assets/MeshImporter.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectDescriptor.h"
#include "Quiet/Assets/QMeshReader.h"

static_assert(
    MESHOPTIMIZER_VERSION == 1020,
    "Quiet_Cooker expects meshoptimizer v1.2."
);

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

    bool SameVertex(const Quiet::Assets::MeshVertex& lhs, const Quiet::Assets::MeshVertex& rhs)
    {
        return
            lhs.Position.x == rhs.Position.x &&
            lhs.Position.y == rhs.Position.y &&
            lhs.Position.z == rhs.Position.z &&

            lhs.Color.x == rhs.Color.x &&
            lhs.Color.y == rhs.Color.y &&
            lhs.Color.z == rhs.Color.z &&

            lhs.Normal.x == rhs.Normal.x &&
            lhs.Normal.y == rhs.Normal.y &&
            lhs.Normal.z == rhs.Normal.z &&

            lhs.TexCoord.x == rhs.TexCoord.x &&
            lhs.TexCoord.y == rhs.TexCoord.y;
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

    bool ValidateQMeshRoundTrip(const std::filesystem::path& path, const Quiet::Assets::MeshImportResult& source, std::string& outError)
    {
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
            if (!SameVertex(source.Vertices[i], decoded.Vertices[i]))
            {
                outError = "QMesh round-trip changed vertex data.";
                return false;
            }
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

    bool OptimizeProjectMeshes(const std::filesystem::path& projectRoot, const std::filesystem::path& outputRoot, const Quiet::Project::AssetRegistry& registry)
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

            const std::filesystem::path cookedPath = outputRoot / "Content" / "Meshes" / (record.ID + ".qmesh");
            Quiet::Cooker::QMeshWriteStats writeStats{};
            std::string writeError;

            if (!Quiet::Cooker::WriteQMesh(cookedPath, mesh, writeStats, writeError))
            {
                std::cerr << "Failed to write mesh '" << record.ID << "': " << writeError << '\n';
                success = false;
                continue;
            }

            std::string validationError;
            if (!ValidateQMeshRoundTrip(cookedPath, mesh, validationError))
            {
                std::cerr << "Failed to validate mesh '" << record.ID << "': " << validationError << '\n';
                success = false;
                continue;
            }

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

            std::cout
                << "  QMesh:      " << cookedPath.filename().string() << '\n'
                << "  Vertex data: " << writeStats.RawVertexBytes << " -> " << writeStats.EncodedVertexBytes << " bytes (" << percentage(writeStats.EncodedVertexBytes, writeStats.RawVertexBytes) << "%)\n"
                << "  Index data:  " << writeStats.RawIndexBytes << " -> " << writeStats.EncodedIndexBytes << " bytes (" << percentage(writeStats.EncodedIndexBytes, writeStats.RawIndexBytes) << "%)\n"
                << "  Raw data:   " << rawGeometryBytes << " bytes\n"
                << "  Encoded:    " << encodedGeometryBytes << " bytes (" << encodedPercent << "%)\n"
                << "  File size:  " << writeStats.FileBytes << " bytes\n"
                << "  Round-trip: OK\n"
                << '\n';

    }

    return success;
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

    if (!OptimizeProjectMeshes(projectRoot, outputRoot, assetRegistry))
    {
        std::cerr << "Mesh optimization completed with errors.\n";
        return 1;
    }

    std::cout << "Mesh optimization completed successfully.\n";
    return 0;
}