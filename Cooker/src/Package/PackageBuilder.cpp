#include "Package/PackageBuilder.h"

#include <utility>

#include "Quiet/UI/UIDocument.h"

namespace Quiet::Cooker
{
    namespace
    {
        bool PackageUIAssets(CookContext& context, std::string& outError)
        {
            namespace fs = std::filesystem;

            for (const auto& record : context.AssetRegistry.Records)
            {
                const bool isDocument = record.Type == Project::AssetType::UIDocument;
                if (record.BuiltIn || (!isDocument && record.Type != Project::AssetType::UIStyleSheet))
                {
                    continue;
                }

                const std::string assetID = Core::UUIDToString(record.ID);
                const auto assetType = std::string(Project::ToString(record.Type));
                if (record.Path.empty())
                {
                    outError = assetType + " '" + assetID + "' has no source path.";
                    return false;
                }

                const fs::path rawPath = record.Path;
                const fs::path sourcePath = (rawPath.is_absolute() ? rawPath : context.ProjectRoot / rawPath).lexically_normal();

                const auto fail = [&](const std::string& reason)
                {
                    outError = "Failed to package " + assetType + " '" + assetID + "' from '" + sourcePath.string() + "': " + reason;
                    return false;
                };

                std::string sourceHash;
                std::string error;
                if (!ComputeFileHash(sourcePath, sourceHash, error))
                    return fail(error);

                if (isDocument)
                {
                    UI::UIDocument document;
                    if (!UI::LoadUIDocumentFromFile(sourcePath, document, error))
                        return fail(error);
                }
                else
                {
                    UI::UIStyleSheet sheet;
                    if (!UI::LoadUIStyleSheetFromFile(sourcePath, sheet, error))
                        return fail(error);
                }

                const char* directory = isDocument ? "UIDocuments" : "UIStyleSheets";
                const char* extension = isDocument ? ".quidoc" : ".quistyle";
                const fs::path runtimePath = fs::path("Content") / directory / (assetID + extension);
                const fs::path cookedPath = context.OutputRoot / runtimePath;

                std::error_code copyError;
                if (!fs::copy_file(sourcePath, cookedPath, fs::copy_options::overwrite_existing, copyError))
                {
                    return fail("Could not copy to '" + cookedPath.string() + "': " + (copyError ? copyError.message() : "no file was copied."));
                }

                std::string cookedHash;
                if (!ComputeFileHash(cookedPath, cookedHash, error))
                    return fail(error);
                if (cookedHash != sourceHash)
                    return fail("The source changed during packaging. Run the Cooker again.");

                context.RuntimeAssets.push_back({
                    record.ID, record.Name, record.Type, runtimePath.generic_string()
                });
            }

            return true;
        }
    }

    bool FinalizeRuntimePackage(const std::filesystem::path& startupScenePath,
                                const Project::ProjectDescriptor& descriptor,
                                CookContext& context,
                                PackageBuildResult& outResult,
                                std::string& outError)
    {
        namespace fs = std::filesystem;

        outResult = {};
        outError.clear();

        if (!PackageUIAssets(context, outError)) return false;

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