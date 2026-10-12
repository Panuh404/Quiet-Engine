#pragma once

#include <filesystem>
#include <string>

#include "Core/CookContext.h"
#include "Package/PackageLayout.h"

#include "Quiet/Project/ProjectDescriptor.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    /**
     * @brief Contains finalized package metadata and nonfatal maintenance warnings.
     * @ingroup quiet_cooker
     */
    struct PackageBuildResult
    {
        Project::RuntimeManifest Manifest;
        std::filesystem::path ManifestPath;

        PackageCleanupStats CleanupStats;

        std::string CleanupWarning;
        std::string CacheWarning;
    };

    /**
     * @brief Copies the startup scene, writes the manifest, and maintains outputs.
     * @ingroup quiet_cooker
     * @return False on scene preparation, copying, or manifest-writing failure.
     * @note Moves context.RuntimeAssets into the assembled manifest.
     * @note Cleanup and cache-save failures are returned as warning strings.
     * @note Does not roll back filesystem changes on failure.
     */
    bool FinalizeRuntimePackage(const std::filesystem::path& startupScenePath,
                                const Project::ProjectDescriptor& descriptor,
                                CookContext& context,
                                PackageBuildResult& outResult,
                                std::string& outError);
}