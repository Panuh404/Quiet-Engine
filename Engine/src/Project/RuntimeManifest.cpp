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

            // Runtime packages use portable forward-slash relative paths.
            static constexpr std::string_view invalidCharacters = "\\:*?\"<>|";

            if (value.front() == '/') return false;
            if (value.find_first_of(invalidCharacters) != std::string_view::npos) return false;

            const std::filesystem::path path{std::string(value)};
            const std::string normalized = path.lexically_normal().generic_string();

            if (normalized.empty() || normalized == ".") return false;

            // Require canonical package-relative representation.
            if (normalized != value) return false;

            for (const auto& segment : path)
            {
                if (segment == "." || segment == "..") return false;
            }

            return true;
        }

        bool ValidateManifest(const RuntimeManifest& manifest)
        {
            if (manifest.Version != CurrentRuntimeManifestVersion || manifest.ProjectName.empty()) return false;
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

        try
        {
            json root;
            input >> root;

            if (!root.is_object()) return false;

            RuntimeManifest manifest{};
            manifest.Version = root.at("Version").get<int>();
            manifest.ProjectName = root.at("ProjectName").get<std::string>();
            manifest.StartupScene = root.at("StartupScene").get<std::string>();

            const json& assets = root.at("Assets");
            if (!assets.is_array()) return false;

            for (const auto& value : assets)
            {
                if (!value.is_object()) return false;

                RuntimeAssetRecord record{};
                record.ID = value.at("ID").get<std::string>();
                record.Path = value.at("Path").get<std::string>();

                const std::string typeName = value.at("Type").get<std::string>();
                AssetType type = AssetType::Unknown;

                if (!AssetTypeFromString(typeName, type)) return false;

                record.Type = type;
                manifest.Assets.push_back(std::move(record));
            }

            if (!ValidateManifest(manifest)) return false;
            SortAssets(manifest);

            outManifest = std::move(manifest);
            return true;
        }
        catch (const json::exception&)
        {
            return false;
        }
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