#include "Package/PackageBuilder.h"

#include <utility>

namespace Quiet::Cooker
{
    bool FinalizeRuntimePackage(const std::filesystem::path& startupScenePath,
                                const Project::ProjectDescriptor& descriptor,
                                CookContext& context,
                                PackageBuildResult& outResult,
                                std::string& outError)
    {
        namespace fs = std::filesystem;

        outResult = {};
        outError.clear();

        const fs::path runtimeScenePath = fs::path("Scenes") / fs::path(descriptor.StartupScene);
        const fs::path cookedScenePath = context.OutputRoot / runtimeScenePath;

        std::error_code ec;
        fs::create_directories(cookedScenePath.parent_path(), ec);
        if (ec)
        {
            outError = "Failed to prepare startup scene output directory: " + ec.message();
            return false;
        }

        fs::copy_file(startupScenePath, cookedScenePath, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            outError = "Failed to copy startup scene: " + ec.message();
            return false;
        }

        Project::RuntimeManifest runtimeManifest{};
        runtimeManifest.ProjectName = descriptor.Name;
        runtimeManifest.StartupScene = runtimeScenePath.generic_string();
        runtimeManifest.Assets = std::move(context.RuntimeAssets);

        const fs::path runtimeManifestPath = context.OutputRoot / "RuntimeManifest.json";
        if (!Project::RuntimeManifestIO::SaveToFile(runtimeManifestPath, runtimeManifest))
        {
            outError = "Failed to write runtime manifest: " + runtimeManifestPath.string();
            return false;
        }

        std::string cleanupError;
        if (!PruneStaleCookedAssets(context.OutputRoot, runtimeManifest, outResult.CleanupStats, cleanupError))
        {
            outResult.CleanupWarning = std::move(cleanupError);
        }

        const fs::path cookCachePath = context.OutputRoot / ".quiet-cook-cache.json";
        std::string cacheError;
        if (!SaveCookCache(cookCachePath, context.NextCache, cacheError))
        {
            outResult.CacheWarning = "Failed to save cook cache '" + cookCachePath.string() + "': " + cacheError;
        }

        outResult.Manifest = std::move(runtimeManifest);
        outResult.ManifestPath = runtimeManifestPath;
        return true;
    }
}