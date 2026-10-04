#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    struct PackageCleanupStats
    {
        std::size_t RemovedMeshes = 0;
        std::size_t RemovedTextures = 0;
        std::size_t RemovedAudio = 0;
        std::size_t RemovedScripts = 0;

        [[nodiscard]] std::size_t TotalRemoved() const
        {
            return RemovedMeshes + RemovedTextures + RemovedAudio + RemovedScripts;
        }
    };

    bool PrepareOutputLayout(const std::filesystem::path& outputRoot, std::string& outError);

    bool PruneStaleCookedAssets(const std::filesystem::path& outputRoot,
                                const Project::RuntimeManifest& manifest,
                                PackageCleanupStats& outStats,
                                std::string& outError);
}
