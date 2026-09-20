#include "Quiet/Project/RuntimeManifest.h"

#include <algorithm>
#include <fstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace Quiet::Project
{
    namespace
    {
        using json = nlohmann::json;

        bool IsSafePackageRelativePath(const std::string_view value)
        {
            if (value.empty()) return false;

            // Runtime manifests always store generic package-relative paths.
            if (value.find('\\') != std::string_view::npos) return false;
            const std::filesystem::path path{value};
            if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;

            for (const auto& segment : path)
            {
                if (segment == "..") return false;
            }

            return true;
        }

        bool ValidateManifest(const RuntimeManifest& manifest)
        {
            if (manifest.Version <= 0 || manifest.ProjectName.empty()) return false;
            if (!IsSafePackageRelativePath(manifest.StartupScene)) return false;

            std::unordered_set<std::string> assetIDs;
            assetIDs.reserve(manifest.Assets.size());

            for (const RuntimeAssetRecord& record : manifest.Assets)
            {
                if (!record.IsValid()) return false;
                if (!IsSafePackageRelativePath(record.Path)) return false;
                if (!assetIDs.insert(record.ID).second) return false;
            }

            return true;
        }

        void SortAssets(RuntimeManifest& manifest)
        {
            std::sort(
                manifest.Assets.begin(),
                manifest.Assets.end(),
                [](const RuntimeAssetRecord& lhs, const RuntimeAssetRecord& rhs)
                {
                    return lhs.ID < rhs.ID;
                }
            );
        }
    }

    bool RuntimeManifestIO::LoadFromFile(const std::filesystem::path& path, RuntimeManifest& outManifest)
    {
        std::ifstream input(path);
        if (!input.is_open()) return false;
        json root;

        try
        {
            input >> root;
        }
        catch (...)
        {
            return false;
        }

        if (!root.is_object()) return false;
        RuntimeManifest manifest{};
        manifest.Version = root.value("Version", 0);
        manifest.ProjectName = root.value("ProjectName", "");
        manifest.StartupScene = root.value("StartupScene", "");

        if (!root.contains("Assets") || !root["Assets"].is_array()) return false;
        for (const auto& value : root["Assets"])
        {
            if (!value.is_object()) return false;

            RuntimeAssetRecord record{};
            record.ID = value.value("ID", "");
            record.Path = value.value("Path", "");

            AssetType type = AssetType::Unknown;
            if (!AssetTypeFromString(value.value("Type", "Unknown"), type)) return false;
            record.Type = type;
            manifest.Assets.push_back(std::move(record));
        }

        if (!ValidateManifest(manifest)) return false;
        SortAssets(manifest);

        outManifest = std::move(manifest);
        return true;
    }

    bool RuntimeManifestIO::SaveToFile(const std::filesystem::path& path, const RuntimeManifest& manifest)
    {
        if (!ValidateManifest(manifest)) return false;

        RuntimeManifest sortedManifest = manifest;
        SortAssets(sortedManifest);
        std::error_code ec;
        const std::filesystem::path parentPath = path.parent_path();

        if (!parentPath.empty())
        {
            std::filesystem::create_directories(parentPath, ec);
            if (ec) return false;
        }

        std::ofstream output(path);
        if (!output.is_open()) return false;

        json root;
        root["Version"] = sortedManifest.Version;
        root["ProjectName"] = sortedManifest.ProjectName;
        root["StartupScene"] = sortedManifest.StartupScene;
        root["Assets"] = json::array();

        for (const RuntimeAssetRecord& record : sortedManifest.Assets)
        {
            root["Assets"].push_back(
            {
                    { "ID", record.ID },
                    { "Type", std::string(ToString(record.Type)) },
                    { "Path", std::filesystem::path(record.Path).lexically_normal().generic_string() }
                }
            );
        }

        output << root.dump(4);
        return output.good();
    }
}