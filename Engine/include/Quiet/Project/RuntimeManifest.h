#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "AssetType.h"
#include "Quiet/Core/UUID.h"

namespace Quiet::Project
{
    inline constexpr int CurrentRuntimeManifestVersion = 2;

    struct RuntimeAssetRecord
    {
        Core::UUID ID{};
        std::string Name;
        AssetType Type = AssetType::Unknown;
        std::string Path;

        [[nodiscard]] bool IsValid() const
        {
            return ID.IsValid() && !Name.empty() && Type != AssetType::Unknown && !Path.empty();
        }
    };

    struct RuntimeManifest
    {
        int Version = CurrentRuntimeManifestVersion;
        std::string ProjectName;
        std::string StartupScene;
        std::vector<RuntimeAssetRecord> Assets;
    };

    class RuntimeManifestIO
    {
    public:
        static bool LoadFromFile(const std::filesystem::path& path, RuntimeManifest& outManifest);
        static bool SaveToFile(const std::filesystem::path& path, const RuntimeManifest& manifest);
    };
}
