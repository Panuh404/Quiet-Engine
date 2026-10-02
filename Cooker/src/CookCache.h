#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "Quiet/Project/AssetType.h"

namespace Quiet::Cooker
{
    inline constexpr std::uint32_t CurrentCookCacheVersion = 1;

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

    struct CookCache
    {
        std::uint32_t Version = CurrentCookCacheVersion;
        std::unordered_map<std::string, CookCacheEntry> Assets;
    };

    bool ComputeFileHash(const std::filesystem::path& path, std::string& outHash, std::string& outError);
    bool LoadCookCache(const std::filesystem::path& path, CookCache& outCache, std::string& outError);
    bool SaveCookCache(const std::filesystem::path& path, const CookCache& cache, std::string& outError);
}
