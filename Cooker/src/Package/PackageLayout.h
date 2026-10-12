#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    /**
     * @brief Counts successfully removed stale files by packaged asset type.
     * @ingroup quiet_cooker
     * @note Counts can describe partial cleanup when pruning fails.
     */
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

    /**
     * @brief Creates Scene and packaged-asset output directories as needed.
     * @ingroup quiet_cooker
     * @return False on reported directory-creation failure.
     * @note Does not empty existing directories or roll back earlier creation.
     */
    bool PrepareOutputLayout(const std::filesystem::path& outputRoot, std::string& outError);

    /**
     * @brief Removes eligible packaged files absent from manifest paths.
     * @ingroup quiet_cooker
     * @return False on reported inspection, enumeration, or removal failure.
     * @note Collects candidates before deletion; removal failures can leave partial cleanup.
     * @note Skips permission-denied traversal and does not remove Scenes or directories.
     * @note Resets statistics and clears outError before processing.
     */
    bool PruneStaleCookedAssets(const std::filesystem::path& outputRoot,
                                const Project::RuntimeManifest& manifest,
                                PackageCleanupStats& outStats,
                                std::string& outError);
}
