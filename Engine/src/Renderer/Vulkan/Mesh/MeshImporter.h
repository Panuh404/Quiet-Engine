#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Renderer/Vulkan/Mesh/MeshImportResult.h"

namespace Quiet::Renderer::Vulkan
{
    class MeshImporter
    {
    public:
        static MeshImportResult LoadFromFile(const std::filesystem::path& path);
    };
}