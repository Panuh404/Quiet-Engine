#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/ProjectDescriptor.h"

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

    bool PrepareOutputLayout(const std::filesystem::path& outputRoot, std::string& outError)
    {
        static constexpr std::array<const char*, 3> directories =
        {
            "Scenes",
            "Content/Meshes",
            "Content/Textures"
        };

        std::error_code ec;

        for (const char* directory : directories)
        {
            std::filesystem::create_directories(outputRoot / directory, ec);
            if (ec)
            {
                outError = "Failed to create output directory '" + (outputRoot / directory).string() + "': " + ec.message();
                return false;
            }
        }

        return true;
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

    if (!PrepareOutputLayout(outputRoot, outputError))
    {
        std::cerr << outputError << '\n';
        return 1;
    }

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
        << "  Audio:      " << counts.Audio << " (deferred)\n"
        << "  Scripts:    " << counts.Scripts << " (deferred)\n"
        << '\n'
        << "Output layout prepared successfully.\n";

    return 0;
}