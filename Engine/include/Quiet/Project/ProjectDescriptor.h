#pragma once

#include <filesystem>
#include <string>

namespace Quiet::Project
{
    struct ProjectDescriptor
    {
        std::string Name = "QuietProject";
        int Version = 1;

        std::string CreatedAtUtc;
        std::string UpdatedAtUtc;

        std::string StartupScene = "DefaultScene.json";
        std::string ScenesDirectory = "Scenes";
        std::string AssetsDirectory = "Assets";
        std::string CacheDirectory = "Cache";
    };

    class ProjectDescriptorIO
    {
    public:
        static ProjectDescriptor MakeDefault(std::string projectName);
        static bool LoadFromFile(const std::filesystem::path& path, ProjectDescriptor& outDescriptor);
        static bool SaveToFile(const std::filesystem::path& path, ProjectDescriptor descriptor);

        [[nodiscard]] static std::string GetCurrentUtcTimestampString();
    };
}