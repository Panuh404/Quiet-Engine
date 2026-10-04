#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Audio/AudioCooker.h"
#include "Core/CookContext.h"
#include "Core/CookCache.h"
#include "Mesh/MeshCooker.h"
#include "Package/PackageBuilder.h"
#include "Package/PackageLayout.h"
#include "Script/ScriptCooker.h"

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectDescriptor.h"
#include "Quiet/Project/RuntimeManifest.h"
#include "Texture/TextureCooker.h"

namespace
{
    struct AssetCounts
    {
        std::size_t Meshes = 0;
        std::size_t Textures = 0;
        std::size_t Audio = 0;
        std::size_t Scripts = 0;
    };

    AssetCounts CountProjectAssets(const Quiet::Project::AssetRegistry& registry)
    {
        AssetCounts counts{};

        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            // Engine-provided assets do not belong in the project package.
            if (record.BuiltIn) continue;
            switch (record.Type)
            {
                case Quiet::Project::AssetType::Mesh:
                    ++counts.Meshes;
                    break;
                case Quiet::Project::AssetType::Texture:
                    ++counts.Textures;
                    break;
                case Quiet::Project::AssetType::Audio:
                    ++counts.Audio;
                    break;
                case Quiet::Project::AssetType::Script:
                    ++counts.Scripts;
                    break;
                case Quiet::Project::AssetType::Unknown:
                default:
                    break;
            }
        }

        return counts;
    }

    void PrintUsage(const char* executable)
    {
        std::cerr << "Usage:\n" << "  " << executable << " <project.quiet> <output-directory>\n";
    }
}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    namespace Project = Quiet::Project;

    if (argc != 3)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string compressonatorError;

    if (!Quiet::Cooker::ValidateCompressonatorBC7(compressonatorError))
    {
        std::cerr << "Compressonator BC7 codec validation failed: " << compressonatorError << '\n';
        return 1;
    }

    std::cout << "Compressonator BC7 codec: OK\n\n";

    std::error_code ec;
    const fs::path projectFilePath = fs::absolute(argv[1], ec).lexically_normal();
    if (ec)
    {
        std::cerr << "Failed to resolve project path: " << ec.message() << '\n';
        return 1;
    }

    if (!fs::is_regular_file(projectFilePath, ec) || ec)
    {
        std::cerr << "Project file does not exist: " << projectFilePath << '\n';
        return 1;
    }

    const fs::path outputRoot = fs::absolute(argv[2], ec).lexically_normal();
    if (ec)
    {
        std::cerr << "Failed to resolve output path: " << ec.message() << '\n';
        return 1;
    }

    Project::ProjectDescriptor descriptor{};
    if (!Project::ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor))
    {
        std::cerr << "Failed to load project descriptor: " << projectFilePath << '\n';
        return 1;
    }

    const fs::path projectRoot = projectFilePath.parent_path();
    const fs::path assetRegistryPath = projectRoot / "AssetRegistry.json";

    Project::AssetRegistry assetRegistry{};
    if (!Project::AssetRegistryIO::LoadFromFile(assetRegistryPath, assetRegistry))
    {
        std::cerr << "Failed to load asset registry: " << assetRegistryPath << '\n';
        return 1;
    }

    const fs::path startupScenePath = projectRoot / descriptor.ScenesDirectory / descriptor.StartupScene;
    if (!fs::is_regular_file(startupScenePath, ec) || ec)
    {
        std::cerr << "Startup scene does not exist: " << startupScenePath << '\n';
        return 1;
    }

    std::string outputError;

    if (!Quiet::Cooker::PrepareOutputLayout(outputRoot, outputError))
    {
        std::cerr << outputError << '\n';
        return 1;
    }

    const fs::path cookCachePath = outputRoot / ".quiet-cook-cache.json";
    Quiet::Cooker::CookCache previousCookCache{};
    std::string cookCacheLoadError;

    if (!Quiet::Cooker::LoadCookCache(cookCachePath, previousCookCache, cookCacheLoadError))
    {
        std::cerr << "Warning: ignoring invalid cook cache '" << cookCachePath << "': " << cookCacheLoadError << '\n';
        previousCookCache = {};
    }

    Quiet::Cooker::CookCache nextCookCache{};
    const AssetCounts counts = CountProjectAssets(assetRegistry);

    std::cout
        << "Quiet Asset Cooker\n"
        << "------------------\n"
        << "Project:      " << descriptor.Name << '\n'
        << "Source:       " << projectRoot << '\n'
        << "Output:       " << outputRoot << '\n'
        << "Startup Scene:" << ' ' << startupScenePath << '\n'
        << '\n'
        << "Cookable assets:\n"
        << "  Meshes:     " << counts.Meshes << '\n'
        << "  Textures:   " << counts.Textures << '\n'
        << "  Audio:      " << counts.Audio << '\n'
        << "  Scripts:    " << counts.Scripts << '\n'
        << '\n'
        << "Output layout prepared successfully.\n";

    std::vector<Project::RuntimeAssetRecord> runtimeAssets;
    Quiet::Cooker::CookContext cookContext
    {
        projectRoot,
        outputRoot,
        assetRegistry,
        previousCookCache,
        nextCookCache,
        runtimeAssets
    };

    // COOKING MESH ASSETS
    std::cout << "\nCooking mesh assets...\n\n";
    if (!Quiet::Cooker::CookProjectMeshes(cookContext))
    {
        std::cerr << "Mesh cooking completed with errors.\n";
        return 1;
    }
    std::cout << "Mesh cooking completed successfully.\n";

    // COOKING TEXTURE ASSETS
    std::cout << "\nCooking texture assets...\n\n";
    if (!Quiet::Cooker::CookProjectTextures(cookContext))
    {
        std::cerr << "Texture cooking completed with errors.\n";
        return 1;
    }
    std::cout << "Texture cooking completed successfully.\n";

    // PACKAGING AUDIO ASSETS
    std::cout << "\nPackaging audio assets...\n\n";
    if (!Quiet::Cooker::CookProjectAudio(cookContext))
    {
        std::cerr << "Audio packaging completed with errors.\n";
        return 1;
    }
    std::cout << "Audio packaging completed successfully.\n";

    // PACKAGING SCRIPT ASSETS
    std::cout << "\nPackaging script assets...\n\n";
    if (!Quiet::Cooker::CookProjectScripts(cookContext))
    {
        std::cerr << "Script packaging completed with errors.\n";
        return 1;
    }
    std::cout << "Script packaging completed successfully.\n";

    Quiet::Cooker::PackageBuildResult packageResult{};
    std::string packageError;
    if (!Quiet::Cooker::FinalizeRuntimePackage(startupScenePath, descriptor, cookContext, packageResult, packageError))
    {
        std::cerr << packageError << '\n';
        return 1;
    }

    if (!packageResult.CleanupWarning.empty())
    {
        std::cerr << "Warning: failed to prune stale cooked assets: " << packageResult.CleanupWarning << '\n';
    }

    if (!packageResult.CacheWarning.empty())
    {
        std::cerr << "Warning: " << packageResult.CacheWarning << '\n';
    }

    std::cout
        << '\n'
        << "Runtime package metadata:\n"
        << "  Startup scene: " << packageResult.Manifest.StartupScene << '\n'
        << "  Runtime assets: " << packageResult.Manifest.Assets.size() << '\n'
        << "  Stale removed:  " << packageResult.CleanupStats.TotalRemoved() << '\n'
        << "    Meshes:       " << packageResult.CleanupStats.RemovedMeshes << '\n'
        << "    Textures:     " << packageResult.CleanupStats.RemovedTextures << '\n'
        << "    Audio:        " << packageResult.CleanupStats.RemovedAudio << '\n'
        << "    Scripts:      " << packageResult.CleanupStats.RemovedScripts << '\n'
        << "  Manifest:       " << packageResult.ManifestPath << '\n'
        << '\n'
        << "Asset cooking completed successfully.\n";
    return 0;
}