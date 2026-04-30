#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "MeshImportResult.h"

namespace Quiet::Renderer::Vulkan
{
    class MeshImporter
    {
    public:
        static MeshImportResult LoadFromFile(const std::filesystem::path& path);
    };
}