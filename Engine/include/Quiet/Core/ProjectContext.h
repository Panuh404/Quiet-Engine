#pragma once

#include <filesystem>

namespace Quiet::Core
{
    class ProjectContext
    {
    public:
        ProjectContext() = default;
        explicit ProjectContext(std::filesystem::path root) : m_RootPath(std::move(root))
        {
            ResetActiveScenePath();
        }

        void SetRootPath(std::filesystem::path root)
        {
            m_RootPath = std::move(root);
            if (m_ActiveScenePath.empty())
            {
                ResetActiveScenePath();
            }
        }


        [[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }
        [[nodiscard]] std::filesystem::path GetScenesPath() const { return m_RootPath / "Scenes"; }
        [[nodiscard]] std::filesystem::path GetDefaultScenePath() const { return GetScenesPath() / "DefaultScene.json"; }
        [[nodiscard]] const std::filesystem::path& GetActiveScenePath() const { return m_ActiveScenePath; }

        void SetActiveScenePath(std::filesystem::path path)
        {
            if (path.empty())
            {
                ResetActiveScenePath();
                return;
            }
            if (path.is_relative()) m_ActiveScenePath = m_RootPath / std::move(path);
            else m_ActiveScenePath = std::move(path);
        }

        void ResetActiveScenePath()
        {
            m_ActiveScenePath = GetDefaultScenePath();
        }

        bool EnsureDirectoriesExist() const
        {
            std::error_code ec;
            std::filesystem::create_directories(GetScenesPath(), ec);
            return !ec;
        }

    private:
        std::filesystem::path m_RootPath;
        std::filesystem::path m_ActiveScenePath;
    };
}