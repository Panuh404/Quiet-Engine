#pragma once

#include <filesystem>

#include "Quiet/Assets/MeshImportResult.h"

namespace Quiet::Assets
{
    class MeshImporter
    {
    public:
        static MeshImportResult LoadFromFile(const std::filesystem::path& path);
    };
}