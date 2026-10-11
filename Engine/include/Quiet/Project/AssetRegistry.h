#pragma once

#include <filesystem>
#include <vector>

#include "AssetRecord.h"

namespace Quiet::Project
{
    /**
     * @brief Stores versioned project asset records.
     * @ingroup quiet_assets
     */
    struct AssetRegistry
    {
        int Version = 1;
        std::vector<AssetRecord> Records;
    };

    /**
     * @brief Provides asset registry persistence and record operations.
     * @ingroup quiet_assets
     *
     * In-memory record changes do not save files or load resources.
     * Record pointers must not be retained across registry mutation.
     */
    class AssetRegistryIO
    {
    public:
        /**
         * @brief Creates a registry containing built-in mesh records.
         */
        [[nodiscard]] static AssetRegistry MakeDefault();

        /**
         * @brief Loads accepted records, restores built-ins, and replaces the output.
         * @return False on open failure, parse failure, or a non-object root.
         * @note Invalid entries can be skipped while loading returns true.
         * @note Field extraction exceptions propagate before output replacement.
         * @note Does not generally deduplicate IDs or validate source files.
         */
        static bool LoadFromFile(const std::filesystem::path& path, AssetRegistry& outRegistry);

        /**
         * @brief Writes registry metadata and valid records as JSON.
         * @return False on open failure; otherwise stream status after writing.
         * @note Does not provide atomic replacement or checked close.
         */
        static bool SaveToFile(const std::filesystem::path& path, const AssetRegistry& registry);

        /**
         * @brief Upserts engine primitive records and sorts by ID.
         * @note Overwrites the first matching record without removing duplicates.
         */
        static void EnsureBuiltInAssets(AssetRegistry& registry);

        /**
         * @brief Replaces the first matching ID or appends a valid record.
         * @return True when accepted, even if the record was unchanged.
         * @note Sorts records; does not save files or load resources.
         */
        static bool AddOrUpdate(AssetRegistry& registry, AssetRecord record);

        /**
         * @brief Removes every record matching an ID.
         * @return True when at least one record was removed.
         * @note Does not delete files, unload resources, or protect built-ins.
         */
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
