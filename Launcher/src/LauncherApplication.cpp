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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, QUI::EditorColor::Background0);

        if (ImGui::Begin("##QuietLauncherRoot", nullptr, rootFlags))
        {
            const float sidebarWidth = uiLayer.Scale(210.0f);
            // ------------------------------------------------------------------
            // Sidebar
            // ------------------------------------------------------------------

            ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background1);
            ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, uiLayer.Scale(ImVec2(22.0f, 22.0f)));

            ImGui::BeginChild("##LauncherSidebar", ImVec2(sidebarWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::AccentHover);
            ImGui::TextUnformatted("QUIET");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("LAUNCHER");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(28.0f)));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::AccentHover);
            ImGui::TextUnformatted("Projects");
            ImGui::PopStyleColor();

            const float bottomBlockHeight = uiLayer.Scale(105.0f);

            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), ImGui::GetWindowHeight() - bottomBlockHeight));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("PROJECTS ROOT");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", m_ProjectRegistry.ProjectsRoot.string().c_str());
            ImGui::PopStyleColor();

            ImGui::EndChild();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0.0f, 0.0f);

            // ------------------------------------------------------------------
            // Main content
            // ------------------------------------------------------------------

            ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, uiLayer.Scale(ImVec2(30.0f, 26.0f)));
            ImGui::BeginChild("##LauncherMain", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

            // Header -----------------------------------------------------------

            if (ImGui::BeginTable("##LauncherHeader", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, uiLayer.Scale(135.0f));

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                ImGui::TextUnformatted("Projects");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextUnformatted("Open or create a Quiet project.");
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);

                ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

                if (QUI::EditorButton("New Project", ImVec2(-1.0f, 0.0f)))
                {
                    m_NewProjectName.fill('\0');
                    m_FeedbackMessage.clear();

                    ImGui::OpenPopup("Create Project");
                }

                ImGui::PopStyleColor(3);
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(12.0f)));

            // Search -----------------------------------------------------------

            const float refreshWidth = uiLayer.Scale(90.0f);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetNextItemWidth(std::max(uiLayer.Scale(160.0f), ImGui::GetContentRegionAvail().x - refreshWidth - spacing));
            ImGui::InputTextWithHint("##ProjectSearch", "Search projects...", m_ProjectSearch.data(), m_ProjectSearch.size());

            ImGui::SameLine();

            if (QUI::EditorButton("Refresh", ImVec2(refreshWidth, 0.0f)))
            {
                RefreshProjects();
            }

            if (!m_FeedbackMessage.empty())
            {
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, m_FeedbackIsError ? QUI::EditorColor::Error : QUI::EditorColor::Success);
                ImGui::TextWrapped("%s", m_FeedbackMessage.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Dummy(ImVec2(0.0f,uiLayer.Scale(16.0f)));

            const std::string search = ToLowerCopy(TrimCopy(m_ProjectSearch.data()));
            const auto matchesSearch = [&](const std::filesystem::path& path)
                {
                    if (search.empty()) return true;
                    return ToLowerCopy(path.generic_string()).find(search) != std::string::npos;
                };

            const auto isValidProject = [](const std::filesystem::path& path)
                {
                    std::error_code ec;
                    return path.extension() == ".quiet" && std::filesystem::exists(path, ec) && !ec;
                };

            const auto isRecentProject = [&](const std::filesystem::path& path)
                {
                    const std::filesystem::path normalized = path.lexically_normal();
                    return std::any_of(
                        m_ProjectRegistry.RecentProjectFiles.begin(),
                        m_ProjectRegistry.RecentProjectFiles.end(),
                        [&](const std::filesystem::path& recent)
                        {
                            return recent.lexically_normal() == normalized;
                        }
                    );
                };

            const auto drawProjectRow = [&](const std::filesystem::path& projectFile)
                {
                    ImGui::PushID(projectFile.generic_string().c_str());

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background1);
                    ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, uiLayer.Scale(5.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, uiLayer.Scale(ImVec2(14.0f, 11.0f)));

                    ImGui::BeginChild("##ProjectCard", ImVec2(0.0f, uiLayer.Scale(66.0f)), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

                    if (ImGui::BeginTable("##ProjectCardLayout", 2, ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, uiLayer.Scale(100.0f));

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                        ImGui::TextUnformatted(projectFile.stem().string().c_str());
                        ImGui::PopStyleColor();

                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                        ImGui::TextUnformatted(projectFile.parent_path().string().c_str());
                        ImGui::PopStyleColor();

                        ImGui::TableSetColumnIndex(1);

                        ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(5.0f)));

                        ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

                        if (QUI::EditorButton("Open", ImVec2(-1.0f, 0.0f)))
                        {
                            LaunchEditor(projectFile);
                        }

                        ImGui::PopStyleColor(3);
                        ImGui::EndTable();
                    }

                    ImGui::EndChild();

                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);

                    ImGui::PopID();
                    ImGui::Spacing();
                };

            // Recent -----------------------------------------------------------

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("Recent");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            bool drewRecentProject = false;
            ImGui::PushID("RecentProjects");

            int recentIndex = 0;

            for (const auto& projectFile : m_ProjectRegistry.RecentProjectFiles)
            {
                if (!isValidProject(projectFile) || !matchesSearch(projectFile))
                {
                    continue;
                }

                ImGui::PushID(recentIndex++);
                drawProjectRow(projectFile);

                ImGui::PopID();
                drewRecentProject = true;
            }

            ImGui::PopID();

            if (!drewRecentProject)
            {
                QUI::DrawMutedText("No recent projects.");
                ImGui::Spacing();
            }

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(12.0f)));

            // All other projects ----------------------------------------------

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("All Projects");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            bool drewProject = false;
            ImGui::PushID("AllProjects");

            int projectIndex = 0;

            for (const auto& projectFile : m_DiscoveredProjects)
            {
                if (!isValidProject(projectFile) || !matchesSearch(projectFile) || isRecentProject(projectFile))
                {
                    continue;
                }

                ImGui::PushID(projectIndex++);
                drawProjectRow(projectFile);

                ImGui::PopID();
                drewProject = true;
            }

            ImGui::PopID();

            if (!drewProject)
            {
                QUI::DrawMutedText(drewRecentProject ? "No additional projects." : "No Quiet projects found.");
            }

            ImGui::EndChild();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        // ----------------------------------------------------------------------
        // Create Project
        // ----------------------------------------------------------------------

        if (ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("Create a new Quiet project");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Choose a name for the project.");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(8.0f)));

            ImGui::SetNextItemWidth(uiLayer.Scale(360.0f));

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::InputTextWithHint("##NewProjectName", "Project name", m_NewProjectName.data(), m_NewProjectName.size());

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextWrapped("Location: %s", m_ProjectRegistry.ProjectsRoot.string().c_str());
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(10.0f)));

            const bool canCreate = !TrimCopy(m_NewProjectName.data()).empty();

            ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

            if (QUI::EditorButton("Create and Open", ImVec2(uiLayer.Scale(160.0f), 0.0f), canCreate))
            {
                std::filesystem::path projectFile;

                if (CreateProject(m_NewProjectName.data(), projectFile))
                {
                    ImGui::CloseCurrentPopup();
                    LaunchEditor(projectFile);
                }
            }

            ImGui::PopStyleColor(3);
            ImGui::SameLine();

            if (QUI::EditorButton("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}