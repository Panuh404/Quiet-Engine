#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Renderer/Vulkan/StaticMesh.h"

namespace Quiet::Renderer::Vulkan
{
    class MeshImporter
    {
    public:
        static bool LoadFromFile(const std::filesystem::path& path,
                                 std::vector<Vertex>& outVertices,
                                 std::vector<std::uint16_t>& outIndices,
                                 std::string& outError);
    };
}