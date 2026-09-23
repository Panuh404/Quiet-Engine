#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Quiet/Assets/MeshData.h"
#include "Quiet/Assets/QMeshFormat.h"

namespace Quiet::Assets
{
    struct QMeshData
    {
        std::vector<MeshVertex> Vertices;
        std::vector<MeshIndex> Indices;
        std::vector<QMeshSubmeshV1> Submeshes;

        MeshBounds Bounds{};
    };

    class QMeshReader
    {
    public:
        static bool LoadFromFile(const std::filesystem::path& path, QMeshData& outMesh, std::string& outError);
    };
}