#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "Quiet/Project/AssetType.h"

namespace Quiet::Cooker
{
    inline constexpr std::uint32_t CurrentCookCacheVersion = 1;

    /**
     * @brief Records fingerprints, conversion signature, and runtime output path.
     * @ingroup quiet_cooker
     * @note IsValid checks field presence and type, not file integrity.
     */
    struct CookCacheEntry
    {
        Project::AssetType Type = Project::AssetType::Unknown;

        std::string SourceHash;
        std::string CookedHash;
        std::string CookSignature;
        std::string RuntimePath;

        [[nodiscard]] bool IsValid() const
        {
            return
                Type != Project::AssetType::Unknown &&
                !SourceHash.empty() &&
                !CookedHash.empty() &&
                !CookSignature.empty() &&
                !RuntimePath.empty();
        }
    };

    /**
     * @brief Stores versioned cooking metadata indexed by asset ID.
     * @ingroup quiet_cooker
     */
    struct CookCache
    {
        std::uint32_t Version = CurrentCookCacheVersion;
        std::unordered_map<std::string, CookCacheEntry> Assets;
    };

    /**
     * @brief Computes a hexadecimal 64-bit FNV-1a fingerprint of file bytes.
     * @ingroup quiet_cooker
     * @return False on reported file-open or read failure.
     * @note Does not fingerprint external source dependencies.
     */
    bool ComputeFileHash(const std::filesystem::path& path, std::string& outHash, std::string& outError);

    /**
     * @brief Loads and validates cache metadata.
     * @ingroup quiet_cooker
     * @note Missing files and incompatible versions produce an empty cache.
     * @note Resets outCache before loading; invalid data returns false.
     */
    bool LoadCookCache(const std::filesystem::path& path, CookCache& outCache, std::string& outError);

    /**
     * @brief Writes current-version cache metadata, omitting invalid entries.
     * @ingroup quiet_cooker
     * @note Truncates the destination without atomic replacement.
     */
    bool SaveCookCache(const std::filesystem::path& path, const CookCache& cache, std::string& outError);
}
