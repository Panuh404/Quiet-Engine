#include "Quiet/Project/ProjectRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>


namespace Quiet::Project
{
    namespace
    {
        using json = nlohmann::json;

        std::filesystem::path GetHomeDirectory()
        {
#if defined(_WIN32)
            if (const char* profile = std::getenv("USERPROFILE"))
            {
                return std::filesystem::path(profile);
            }
            if (const char* homeDrive = std::getenv("HOMEDRIVE"))
            {
                if (const char* homePath = std::getenv("HOMEPATH"))
                {
                    return std::filesystem::path(std::string(homeDrive) + std::string(homePath));
                }
            }
#else
            if (const char* home = std::getenv("HOME"))
            {
                return std::filesystem::path(home);
            }
#endif
            return std::filesystem::current_path();
        }
    }

    std::filesystem::path ProjectRegistryIO::GetDefaultProjectsRootPath()
    {
        return GetHomeDirectory() / "Documents" / "QuietProjects";
    }

    std::filesystem::path ProjectRegistryIO::GetDefaultRegistryFilePath()
    {
        return GetHomeDirectory() / ".quiet" / "ProjectRegistry.json";
    }

     ProjectRegistry ProjectRegistryIO::MakeDefault()
    {
        ProjectRegistry registry{};
        registry.ProjectsRoot = GetDefaultProjectsRootPath();
        return registry;
    }

    bool ProjectRegistryIO::LoadFromFile(const std::filesystem::path& path, ProjectRegistry& outRegistry)
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

        ProjectRegistry registry = MakeDefault();
        const std::string projectsRoot = root.value("ProjectsRoot", "");
        const std::string lastOpened = root.value("LastOpenedProjectFile", "");

        if (!projectsRoot.empty()) registry.ProjectsRoot = projectsRoot;
        if (!lastOpened.empty()) registry.LastOpenedProjectFile = lastOpened;

        if (root.contains("RecentProjectFiles") && root["RecentProjectFiles"].is_array())
        {
            registry.RecentProjectFiles.clear();
            for (const auto& value : root["RecentProjectFiles"])
            {
                if (value.is_string())
                {
                    registry.RecentProjectFiles.emplace_back(value.get<std::string>());
                }
            }
        }

        outRegistry = std::move(registry);
        return true;
    }

    bool ProjectRegistryIO::SaveToFile(const std::filesystem::path& path, const ProjectRegistry& registry)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream output(path);
        if (!output.is_open()) return false;

        json root;
        root["ProjectsRoot"] = registry.ProjectsRoot.string();
        root["LastOpenedProjectFile"] = registry.LastOpenedProjectFile.string();

        json recent = json::array();
        for (const auto& projectFile : registry.RecentProjectFiles)
        {
            recent.push_back(projectFile.string());
        }

        root["RecentProjectFiles"] = std::move(recent);

        output << root.dump(4);
        return output.good();
    }

    void ProjectRegistryIO::TouchRecentProject(ProjectRegistry& registry,
                                               const std::filesystem::path& projectFilePath,
                                               const std::size_t maxEntries)
    {
        registry.LastOpenedProjectFile = projectFilePath;
        std::erase(registry.RecentProjectFiles, projectFilePath);

        registry.RecentProjectFiles.insert(registry.RecentProjectFiles.begin(), projectFilePath);
        if (registry.RecentProjectFiles.size() > maxEntries)
        {
            registry.RecentProjectFiles.resize(maxEntries);
        }
    }

    std::vector<std::filesystem::path> ProjectRegistryIO::EnumerateProjectFiles(const std::filesystem::path &projectsRoot)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!std::filesystem::exists(projectsRoot, ec)) return result;

        for (std::filesystem::recursive_directory_iterator it(projectsRoot, ec), end; it != end; it.increment(ec))
        {
            if (ec) break;

            if (!it->is_regular_file(ec)) continue;

            const std::filesystem::path path = it->path();
            if (path.extension() == ".quiet")
            {
                result.push_back(path);
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }
}

