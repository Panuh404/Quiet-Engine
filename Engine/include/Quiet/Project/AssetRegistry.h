#pragma once

#include <filesystem>
#include <vector>

#include "AssetRecord.h"

namespace Quiet::Project
{
    struct AssetRegistry
    {
        int Version = 1;
        std::vector<AssetRecord> Records;
    };

    class AssetRegistryIO
    {
    public:
        [[nodiscard]] static AssetRegistry MakeDefault();

        static bool LoadFromFile(const std::filesystem::path& path, AssetRegistry& outRegistry);
        static bool SaveToFile(const std::filesystem::path& path, const AssetRegistry& registry);

        static void EnsureBuiltInAssets(AssetRegistry& registry);

        static bool AddOrUpdate(AssetRegistry& registry, AssetRecord record);
        static bool RemoveByID(AssetRegistry& registry, std::string_view id);

        [[nodiscard]] static const AssetRecord* FindByID(const AssetRegistry& registry, std::string_view id);
        [[nodiscard]] static std::vector<const AssetRecord*> GetAssetsByType(
            const AssetRegistry& registry,
            AssetType type
        );
        [[nodiscard]] static std::vector<std::string> GetAssetIDsByType(
            const AssetRegistry& registry,
            AssetType type
        );
    };
}
