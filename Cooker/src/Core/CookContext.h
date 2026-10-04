#pragma once

#include <filesystem>
#include <vector>

#include "Core/CookCache.h"

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    struct CookContext
    {
        const std::filesystem::path& ProjectRoot;
        const std::filesystem::path& OutputRoot;

        const Project::AssetRegistry& AssetRegistry;

        const CookCache& PreviousCache;
        CookCache& NextCache;

        std::vector<Project::RuntimeAssetRecord>& RuntimeAssets;
    };
}