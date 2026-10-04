#include "Package/PackageLayout.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Quiet::Cooker
{
    namespace
    {
        bool IsSupportedPackagedAudioFile(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](const unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });

            return extension == ".wav" || extension == ".flac" || extension == ".mp3";
        }
    }

    bool PrepareOutputLayout(const std::filesystem::path& outputRoot, std::string& outError)
    {
        static constexpr std::array<const char*, 5> directories =
        {
            "Scenes",
            "Content/Meshes",
            "Content/Textures",
            "Content/Audio",
            "Content/Scripts",
        };

        outError.clear();
        std::error_code ec;
        for (const char* directory : directories)
        {
            const std::filesystem::path path = outputRoot / directory;
            std::filesystem::create_directories(path, ec);
            if (ec)
            {
                outError = "Failed to create output directory '" + path.string() + "': " + ec.message();
                return false;
            }
        }

        return true;
    }

    bool PruneStaleCookedAssets(const std::filesystem::path& outputRoot,
                                const Project::RuntimeManifest& manifest,
                                PackageCleanupStats& outStats,
                                std::string& outError)
    {
        outStats = {};
        outError.clear();

        std::unordered_set<std::string> referencedAssets;
        referencedAssets.reserve(manifest.Assets.size());

        for (const Project::RuntimeAssetRecord& record : manifest.Assets)
        {
            if (record.Type != Project::AssetType::Mesh &&
                record.Type != Project::AssetType::Texture &&
                record.Type != Project::AssetType::Audio &&
                record.Type != Project::AssetType::Script)
                continue;

            referencedAssets.insert(std::filesystem::path(record.Path).lexically_normal().generic_string());
        }

        struct CleanupTarget
        {
            std::filesystem::path Directory;
            std::string_view Extension;
            Project::AssetType Type = Project::AssetType::Unknown;
        };

        const std::array<CleanupTarget, 4> targets =
        {{
            { outputRoot / "Content" / "Meshes", ".qmesh", Project::AssetType::Mesh },
            { outputRoot / "Content" / "Textures", ".qtexture", Project::AssetType::Texture },
            { outputRoot / "Content" / "Audio", {}, Project::AssetType::Audio },
            { outputRoot / "Content" / "Scripts", ".lua", Project::AssetType::Script }
        }};

        std::vector<std::pair<std::filesystem::path, Project::AssetType>> staleFiles;

        for (const CleanupTarget& target : targets)
        {
            std::error_code ec;
            if (!std::filesystem::exists(target.Directory, ec))
            {
                if (ec)
                {
                    outError = "Failed to inspect cooked asset directory '" + target.Directory.string() + "': " + ec.message();
                    return false;
                }

                continue;
            }

            std::filesystem::recursive_directory_iterator iterator(target.Directory, std::filesystem::directory_options::skip_permission_denied, ec);
            if (ec)
            {
                outError = "Failed to enumerate cooked asset directory '" + target.Directory.string() + "': " + ec.message();
                return false;
            }

            const std::filesystem::recursive_directory_iterator end;
            while (iterator != end)
            {
                const std::filesystem::directory_entry entry = *iterator;
                ec.clear();
                const bool isRegularFile = entry.is_regular_file(ec);
                if (ec)
                {
                    outError = "Failed to inspect cooked asset '" + entry.path().string() + "': " + ec.message();
                    return false;
                }

                const bool supportedTargetFile = target.Type == Project::AssetType::Audio
                    ? IsSupportedPackagedAudioFile(entry.path())
                    : entry.path().extension() == target.Extension;

                if (isRegularFile && supportedTargetFile)
                {
                    const std::filesystem::path relativePath = entry.path().lexically_relative(outputRoot);
                    if (relativePath.empty())
                    {
                        outError = "Failed to resolve cooked asset relative path: " + entry.path().string();
                        return false;
                    }

                    const std::string runtimePath = relativePath.lexically_normal().generic_string();
                    if (!referencedAssets.contains(runtimePath))
                    {
                        staleFiles.emplace_back(entry.path(), target.Type);
                    }
                }

                iterator.increment(ec);
                if (ec)
                {
                    outError = "Failed while enumerating cooked assets in '" + target.Directory.string() + "': " + ec.message();
                    return false;
                }
            }
        }

        for (const auto& [path, type] : staleFiles)
        {
            std::error_code ec;
            const bool removed = std::filesystem::remove(path, ec);
            if (ec || !removed)
            {
                outError = "Failed to remove stale cooked asset '" + path.string() + "'" + (ec ? ": " + ec.message() : ".");
                return false;
            }

            switch (type)
            {
                case Project::AssetType::Mesh:      ++outStats.RemovedMeshes; break;
                case Project::AssetType::Texture:   ++outStats.RemovedTextures; break;
                case Project::AssetType::Audio:     ++outStats.RemovedAudio; break;
                case Project::AssetType::Script:    ++outStats.RemovedScripts; break;
                default:
                    break;
            }
        }

        return true;
    }
}