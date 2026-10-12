#pragma once

#include <filesystem>
#include <vector>

#include "Core/CookCache.h"

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    /**
     * @brief Supplies borrowed inputs and mutable outputs to cooking stages.
     * @ingroup quiet_cooker
     * @note All referenced objects must outlive stage execution.
     * @note PreviousCache is read-only; stages populate NextCache and RuntimeAssets.
     */
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