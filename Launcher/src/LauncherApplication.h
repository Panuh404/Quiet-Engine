#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Quiet/Project/ProjectRegistry.h"

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Renderer::UI
{
    class ImGuiLayer;
}

namespace Quiet::Launcher
{
    class LauncherRenderer;

    class LauncherApplication
    {
    public:
        LauncherApplication();
        ~LauncherApplication();

        LauncherApplication(const LauncherApplication&) = delete;
        LauncherApplication& operator=(const LauncherApplication&) = delete;
        LauncherApplication(LauncherApplication&&) = delete;
        LauncherApplication& operator=(LauncherApplication&&) = delete;

        int Run();

    private:
        void LoadOrCreateProjectRegistry();
        void RefreshProjects();

        void BuildLauncherUI(Renderer::UI::ImGuiLayer& uiLayer);

        bool CreateProject(const std::string& projectName, std::filesystem::path& outProjectFile);

        bool LaunchEditor(const std::filesystem::path& projectFile);

    private:
        std::unique_ptr<Platform::Window> m_Window;
        std::unique_ptr<LauncherRenderer> m_Renderer;
        Project::ProjectRegistry m_ProjectRegistry{};
        std::filesystem::path m_ProjectRegistryPath;
        std::vector<std::filesystem::path> m_DiscoveredProjects;

        std::array<char, 160> m_ProjectSearch{};
        std::array<char, 128> m_NewProjectName{};

        std::string m_FeedbackMessage;

        bool m_FeedbackIsError = false;
        bool m_ExitRequested = false;
    };
}