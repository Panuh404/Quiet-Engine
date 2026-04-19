#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include "ProjectDescriptor.h"

namespace Quiet::Project
{
    class ProjectContext
    {
    public:
        ProjectContext() = default;

        void Initialize(std::filesystem::path projectFilePath, ProjectDescriptor descriptor)
        {
            m_ProjectFilePath = std::move(projectFilePath);
            m_RootPath = m_ProjectFilePath.parent_path();
            m_ProjectDescriptor = std::move(descriptor);
            ResetActiveScenePath();
        }

        [[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }
        [[nodiscard]] const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
        [[nodiscard]] const ProjectDescriptor& GetProjectDescriptor() const { return m_ProjectDescriptor; }
        [[nodiscard]] const std::string& GetProjectName() const { return m_ProjectDescriptor.Name; }
        [[nodiscard]] std::filesystem::path GetScenesPath() const { return m_RootPath / m_ProjectDescriptor.ScenesDirectory; }
        [[nodiscard]] std::filesystem::path GetAssetsPath() const { return m_RootPath / m_ProjectDescriptor.AssetsDirectory; }
        [[nodiscard]] std::filesystem::path GetCachePath() const { return m_RootPath / m_ProjectDescriptor.CacheDirectory; }
        [[nodiscard]] std::filesystem::path GetStartupScenePath() const { return GetScenesPath() / m_ProjectDescriptor.StartupScene; }
        [[nodiscard]] const std::filesystem::path& GetActiveScenePath() const { return m_ActiveScenePath; }

        void SetActiveScenePath(std::filesystem::path path)
        {
            if (path.empty())
            {
                ResetActiveScenePath();
                return;
            }

            if (path.is_relative())
            {
                m_ActiveScenePath = GetScenesPath() / std::move(path);
            }
            else
            {
                m_ActiveScenePath = std::move(path);
            }
        }

        void ResetActiveScenePath()
        {
            m_ActiveScenePath = GetStartupScenePath();
        }

        bool EnsureDirectoriesExist() const
        {
            std::error_code ec;
            std::filesystem::create_directories(GetScenesPath(), ec);
            if (ec) return false;

            std::filesystem::create_directories(GetAssetsPath(), ec);
            if (ec) return false;

            std::filesystem::create_directories(GetCachePath(), ec);
            return !ec;
        }

    private:
        std::filesystem::path m_RootPath;
        std::filesystem::path m_ProjectFilePath;
        std::filesystem::path m_ActiveScenePath;
        ProjectDescriptor m_ProjectDescriptor{};
    };

    bool LoadOrCreateProject(ProjectContext& projectContext, const std::filesystem::path& projectFilePath);
}