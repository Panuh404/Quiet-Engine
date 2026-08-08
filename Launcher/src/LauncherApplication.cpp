#include "LauncherApplication.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "imgui.h"

#include "LauncherRenderer.h"

#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Shell.h"
#include "Quiet/Platform/Window.h"
#include "Quiet/Project/ProjectContext.h"

#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    std::string TrimCopy(std::string value)
    {
        const auto isSpace = [](const unsigned char c)
        {
            return std::isspace(c) != 0;
        };

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }

        return value;
    }

    bool IsValidProjectName(const std::string& name)
    {
        if (name.empty() || name == "." || name == "..")
        {
            return false;
        }

        static constexpr const char* invalidCharacters = "\\/:*?\"<>|";
        return name.find_first_of(invalidCharacters) == std::string::npos;
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );
        return value;
    }
}

namespace Quiet::Launcher
{
    LauncherApplication::LauncherApplication()
    {
        Platform::WindowDesc windowDesc{};

        windowDesc.Width = 1100;
        windowDesc.Height = 700;
        windowDesc.Title = "Quiet Launcher";
        windowDesc.IconPath = "Resources/Textures/Editor/quiet-icon.png";
        windowDesc.Resizable = true;
        windowDesc.StartMaximized = false;

        m_Window = std::make_unique<Platform::Window>(windowDesc);
        if (!m_Window->IsVulkanSupported())
        {
            throw std::runtime_error("GLFW reports that Vulkan is not available. Check the Vulkan loader, drivers, or SDK.");
        }

        m_Renderer = std::make_unique<LauncherRenderer>(*m_Window);

        LoadOrCreateProjectRegistry();
        RefreshProjects();

        QUIET_CORE_INFO("Initialized Quiet Launcher.");
    }

    LauncherApplication::~LauncherApplication() = default;

    int LauncherApplication::Run()
    {
        QUIET_CORE_INFO("Starting Quiet Launcher.");

        while (!m_ExitRequested && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();

            auto& uiLayer = m_Renderer->GetImGuiLayer();
            uiLayer.BeginFrame();
            BuildLauncherUI(uiLayer);
            uiLayer.EndFrame();

            const bool frameCompleted = m_Renderer->DrawFrame();

            if (frameCompleted && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }

        m_Renderer->WaitIdle();

        QUIET_CORE_INFO("Closing Quiet Launcher.");
        return 0;
    }

    void LauncherApplication::LoadOrCreateProjectRegistry()
    {
        m_ProjectRegistryPath = Project::ProjectRegistryIO::GetDefaultRegistryFilePath();
        if (std::filesystem::exists(m_ProjectRegistryPath))
        {
            if (!Project::ProjectRegistryIO::LoadFromFile(m_ProjectRegistryPath, m_ProjectRegistry))
            {
                throw std::runtime_error("Failed to load Quiet project registry.");
            }
        }
        else
        {
            m_ProjectRegistry = Project::ProjectRegistryIO::MakeDefault();

            std::error_code ec;
            std::filesystem::create_directories(m_ProjectRegistry.ProjectsRoot, ec);
            if (ec || !Project::ProjectRegistryIO::SaveToFile(m_ProjectRegistryPath, m_ProjectRegistry))
            {
                throw std::runtime_error("Failed to create Quiet project registry.");
            }
        }

        std::error_code ec;
        std::filesystem::create_directories(m_ProjectRegistry.ProjectsRoot, ec);
        if (ec)
        {
            throw std::runtime_error("Failed to create Quiet projects root.");
        }
    }

    void LauncherApplication::RefreshProjects()
    {
        m_DiscoveredProjects = Project::ProjectRegistryIO::EnumerateProjectFiles(m_ProjectRegistry.ProjectsRoot);
    }

    bool LauncherApplication::CreateProject(const std::string& projectName, std::filesystem::path& outProjectFile)
    {
        outProjectFile.clear();

        const std::string trimmedName = TrimCopy(projectName);
        if (!IsValidProjectName(trimmedName))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "Enter a valid project name.";
            return false;
        }

        const std::filesystem::path projectFolder = m_ProjectRegistry.ProjectsRoot / trimmedName;
        const std::filesystem::path projectFile = projectFolder / (trimmedName + ".quiet");

        if (std::filesystem::exists(projectFile))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "A project with that name already exists.";
            return false;
        }

        Project::ProjectContext projectContext;
        if (!Project::LoadOrCreateProject(projectContext, projectFile))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "Failed to create the project.";
            return false;
        }

        outProjectFile = projectFile.lexically_normal();
        RefreshProjects();

        m_FeedbackIsError = false;
        m_FeedbackMessage = "Project created successfully.";
        return true;
    }

    bool LauncherApplication::LaunchEditor(const std::filesystem::path& projectFile)
    {
        if (projectFile.empty() || !std::filesystem::exists(projectFile))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "The selected project no longer exists.";
            RefreshProjects();
            return false;
        }

        const std::filesystem::path launcherExecutable = Platform::GetExecutablePath();
        if (launcherExecutable.empty())
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "Could not resolve the launcher executable path.";
            return false;
        }

#if defined(_WIN32)
        constexpr const char* editorExecutableName = "Quiet_Editor.exe";
#else
        constexpr const char* editorExecutableName = "Quiet_Editor";
#endif

        const std::filesystem::path editorExecutable = launcherExecutable.parent_path() / editorExecutableName;
        if (!Platform::LaunchDetachedProcess(editorExecutable,
            { std::filesystem::absolute(projectFile).lexically_normal().string()}))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "Quiet Editor could not be launched. Make sure Quiet_Editor is built beside Quiet_Launcher.";
            return false;
        }

        QUIET_CORE_INFO("Launching Quiet Editor for project '{}'.", projectFile.string());
        m_ExitRequested = true;
        return true;
    }

    void LauncherApplication::BuildLauncherUI(Renderer::UI::ImGuiLayer& uiLayer)
    {
        namespace QUI = Renderer::UI;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport) return;

        ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

        const ImGuiWindowFlags rootFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, QUI::EditorColor::Background0);

        if (ImGui::Begin("##QuietLauncherRoot", nullptr, rootFlags))
        {
            const float margin = uiLayer.Scale(32.0f);
            const float availableWidth = std::max(0.0f, ImGui::GetContentRegionAvail().x - margin * 2.0f);
            const float contentWidth = std::min(uiLayer.Scale(1180.0f), availableWidth);
            const float centeredOffset = std::max(0.0f, (ImGui::GetContentRegionAvail().x - contentWidth) * 0.5f);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centeredOffset);

            ImGui::BeginChild("##LauncherContent", ImVec2(contentWidth, 0.0f), ImGuiChildFlags_None);

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(26.0f)));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::AccentHover);
            ImGui::TextUnformatted("QUIET");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("Project Launcher");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Create or open a Quiet project.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (!m_FeedbackMessage.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, m_FeedbackIsError ? QUI::EditorColor::Error : QUI::EditorColor::Success);
                ImGui::TextWrapped("%s", m_FeedbackMessage.c_str());
                ImGui::PopStyleColor();

                ImGui::Spacing();
            }

            ImGui::SetNextItemWidth(uiLayer.Scale(360.0f));

            ImGui::InputTextWithHint("##ProjectSearch", "Search projects...", m_ProjectSearch.data(), m_ProjectSearch.size());

            ImGui::SameLine();

            if (QUI::EditorButton("Refresh"))
            {
                RefreshProjects();
            }

            ImGui::SameLine();

            if (QUI::EditorButton("New Project"))
            {
                m_NewProjectName.fill('\0');
                m_FeedbackMessage.clear();

                ImGui::OpenPopup("Create Project");
            }

            ImGui::Spacing();

            const std::string search = ToLowerCopy(TrimCopy(m_ProjectSearch.data()));
            const auto matchesSearch = [&](const std::filesystem::path& path)
            {
                if (search.empty()) return true;
                return ToLowerCopy(path.generic_string()).find(search) != std::string::npos;
            };

            const auto drawProjectRow = [&](const std::filesystem::path& projectFile)
            {
                ImGui::PushID(projectFile.generic_string().c_str());

                ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);

                ImGui::BeginChild("##ProjectRow", ImVec2(0.0f, uiLayer.Scale(66.0f)), ImGuiChildFlags_Borders);

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                ImGui::TextUnformatted(projectFile.stem().string().c_str());
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextUnformatted(projectFile.parent_path().string().c_str());
                ImGui::PopStyleColor();

                const float buttonWidth = uiLayer.Scale(90.0f);

                ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonWidth));

                if (QUI::EditorButton("Open", ImVec2(buttonWidth, 0.0f)))
                {
                    LaunchEditor(projectFile);
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::PopID();

                ImGui::Spacing();
                };

            QUI::DrawSectionTitle("Recent Projects");

            bool drewRecentProject = false;

            for (const auto& projectFile : m_ProjectRegistry.RecentProjectFiles)
            {
                if (!std::filesystem::exists(projectFile) || !matchesSearch(projectFile))
                {
                    continue;
                }

                drawProjectRow(projectFile);
                drewRecentProject = true;
            }

            if (!drewRecentProject)
            {
                QUI::DrawMutedText("No recent projects.");
                ImGui::Spacing();
            }

            QUI::DrawSectionTitle("All Projects");

            bool drewProject = false;

            for (const auto& projectFile : m_DiscoveredProjects)
            {
                if (!matchesSearch(projectFile)) continue;
                drawProjectRow(projectFile);
                drewProject = true;
            }

            if (!drewProject)
            {
                QUI::DrawMutedText("No Quiet projects found.");
            }

            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Create a new Quiet project.");

            ImGui::Spacing();

            ImGui::SetNextItemWidth(uiLayer.Scale(340.0f));

            ImGui::InputTextWithHint("##NewProjectName", "Project name", m_NewProjectName.data(), m_NewProjectName.size());

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Location: %s", m_ProjectRegistry.ProjectsRoot.string().c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            const bool canCreate = !TrimCopy(m_NewProjectName.data()).empty();
            if (QUI::EditorButton("Create and Open", ImVec2(uiLayer.Scale(160.0f), 0.0f), canCreate))
            {
                std::filesystem::path projectFile;
                if (CreateProject(m_NewProjectName.data(), projectFile))
                {
                    ImGui::CloseCurrentPopup();
                    LaunchEditor(projectFile);
                }
            }

            ImGui::SameLine();

            if (QUI::EditorButton("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}