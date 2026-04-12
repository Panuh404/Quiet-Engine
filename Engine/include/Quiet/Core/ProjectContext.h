#pragma once

#include <filesystem>

namespace Quiet::Core
{
    class ProjectContext
    {
    public:
        ProjectContext() = default;
        explicit ProjectContext(std::filesystem::path root)
            : m_RootPath(std::move(root))
        {
        }

        void SetRootPath(std::filesystem::path root)
        {
            m_RootPath = std::move(root);
        }

        [[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }
        [[nodiscard]] std::filesystem::path GetScenesPath() const { return m_RootPath / "Scenes"; }
        [[nodiscard]] std::filesystem::path GetDefaultScenePath() const { return GetScenesPath() / "DefaultScene.json"; }

        bool EnsureDirectoriesExist() const
        {
            std::error_code ec;
            std::filesystem::create_directories(GetScenesPath(), ec);
            return !ec;
        }

    private:
        std::filesystem::path m_RootPath;
    };
}