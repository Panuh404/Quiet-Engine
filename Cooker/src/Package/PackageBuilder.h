#pragma once

#include <filesystem>
#include <string>

#include "Core/CookContext.h"
#include "Package/PackageLayout.h"

#include "Quiet/Project/ProjectDescriptor.h"
#include "Quiet/Project/RuntimeManifest.h"

namespace Quiet::Cooker
{
    struct PackageBuildResult
    {
        Project::RuntimeManifest Manifest;
        std::filesystem::path ManifestPath;

        PackageCleanupStats CleanupStats;

        std::string CleanupWarning;
        std::string CacheWarning;
    };

    bool FinalizeRuntimePackage(const std::filesystem::path& startupScenePath,
                                const Project::ProjectDescriptor& descriptor,
                                CookContext& context,
                                PackageBuildResult& outResult,
                                std::string& outError);
}