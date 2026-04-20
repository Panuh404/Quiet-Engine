#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Quiet::Project
{
    struct ProjectRegistry
    {
        std::filesystem::path ProjectsRoot;
        std::filesystem::path LastOpenedProjectFile;
        std::vector<std::filesystem::path> RecentProjectFiles;
    };

    class ProjectRegistryIO
    {
    public:
        [[nodiscard]] static std::filesystem::path GetDefaultRegistryFilePath();
        [[nodiscard]] static std::filesystem::path GetDefaultProjectsRootPath();

        static ProjectRegistry MakeDefault();
        static bool LoadFromFile(const std::filesystem::path& path, ProjectRegistry& outRegistry);
        static bool SaveToFile(const std::filesystem::path& path, const ProjectRegistry& registry);

        static void TouchRecentProject(ProjectRegistry& registry,
                                       const std::filesystem::path& projectFilePath,
                                       std::size_t maxEntries = 12);

        [[nodiscard]] static std::vector<std::filesystem::path> EnumerateProjectFiles(const std::filesystem::path& projectsRoot);
    };
}