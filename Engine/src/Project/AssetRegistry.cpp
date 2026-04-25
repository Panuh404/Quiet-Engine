#include "Quiet/Project/AssetRegistry.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace Quiet::Project
{
    namespace
    {
        using json = nlohmann::json;

        void UpsertRecord(AssetRegistry& registry, AssetRecord record)
        {
            auto it = std::find_if(
                registry.Records.begin(),
                registry.Records.end(),
                [&](const AssetRecord& existing)
                {
                    return existing.ID == record.ID;
                }
            );

            if (it != registry.Records.end())
            {
                *it = std::move(record);
                return;
            }

            registry.Records.push_back(std::move(record));
        }

        void SortRecords(AssetRegistry& registry)
        {
            std::sort(
                registry.Records.begin(),
                registry.Records.end(),
                [](const AssetRecord& lhs, const AssetRecord& rhs)
                {
                    return lhs.ID < rhs.ID;
                }
            );
        }
    }

    AssetRegistry AssetRegistryIO::MakeDefault()
    {
        AssetRegistry registry{};
        EnsureBuiltInAssets(registry);
        return registry;
    }

    bool AssetRegistryIO::LoadFromFile(const std::filesystem::path &path, AssetRegistry &outRegistry)
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

        AssetRegistry registry{};
        registry.Version = root.value("Version", registry.Version);

        if (root.contains("Assets") && root["Assets"].is_array())
        {
            for (const auto& value : root["Assets"])
            {
                if (!value.is_object()) continue;

                AssetRecord record{};
                record.ID = value.value("ID", "");
                record.Path = value.value("Path", "");
                record.BuiltIn = value.value("BuiltIn", false);

                AssetType type = AssetType::Unknown;
                if (!AssetTypeFromString(value.value("Type", "Unknown"), type))
                {
                    type = AssetType::Unknown;
                }

                record.Type = type;
                if (!record.IsValid()) continue;
                registry.Records.push_back(std::move(record));
            }
        }
        EnsureBuiltInAssets(registry);
        SortRecords(registry);

        outRegistry = std::move(registry);
        return true;
    }

    bool AssetRegistryIO::SaveToFile(const std::filesystem::path &path, const AssetRegistry &registry)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream output(path);
        if (!output.is_open()) return false;

        json root;
        root["Version"] = registry.Version;
        root["Assets"] = json::array();

        for (const auto& record : registry.Records)
        {
            if (!record.IsValid()) continue;

            root["Assets"].push_back(
                {
                    { "ID", record.ID },
                    { "Type", std::string(ToString(record.Type)) },
                    { "Path", record.Path },
                    { "BuiltIn", record.BuiltIn }
                }
            );
        }
        output << root.dump(4);
        return output.good();
    }

    void AssetRegistryIO::EnsureBuiltInAssets(AssetRegistry &registry)
    {
        UpsertRecord(registry, AssetRecord{ "Cube", AssetType::Mesh, "", true } );
        UpsertRecord(registry, AssetRecord{ "Quad", AssetType::Mesh, "", true } );
        SortRecords(registry);
    }

    bool AssetRegistryIO::AddOrUpdate(AssetRegistry &registry, AssetRecord record)
    {
        if (!record.IsValid()) return false;
        UpsertRecord(registry, std::move(record));
        SortRecords(registry);
        return true;
    }

    bool AssetRegistryIO::RemoveByID(AssetRegistry &registry, std::string_view id)
    {
        const auto it = std::remove_if(
            registry.Records.begin(),
            registry.Records.end(),
            [&](const AssetRecord& record)
            {
                return record.ID == id;
            });

        if (it == registry.Records.end()) return false;
        registry.Records.erase(it, registry.Records.end());
        SortRecords(registry);
        return true;
    }

    const AssetRecord * AssetRegistryIO::FindByID(const AssetRegistry &registry, std::string_view id)
    {
        const auto it = std::find_if(
                    registry.Records.begin(),
                    registry.Records.end(),
                    [&](const AssetRecord& record)
                    {
                        return record.ID == id;
                    }
                );

        if (it == registry.Records.end()) return nullptr;
        return &(*it);
    }

    std::vector<const AssetRecord *> AssetRegistryIO::GetAssetsByType(const AssetRegistry &registry, AssetType type)
    {
        std::vector<const AssetRecord*> result;
        result.reserve(registry.Records.size());

        for (const auto& record : registry.Records)
        {
            if (record.Type == type && record.IsValid())
                result.push_back(&record);
        }
        return result;
    }

    std::vector<std::string> AssetRegistryIO::GetAssetIDsByType(const AssetRegistry &registry, AssetType type)
    {
        std::vector<std::string> result;
        result.reserve(registry.Records.size());

        for (const auto& record : registry.Records)
        {
            if (record.Type == type && record.IsValid())
                result.push_back(record.ID);
        }
        std::sort(result.begin(), result.end());
        return result;
    }
}
