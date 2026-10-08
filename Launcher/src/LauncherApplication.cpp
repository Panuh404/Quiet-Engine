#include "LauncherApplication.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

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

        std::error_code ec;
        if (std::filesystem::exists(projectFolder, ec))
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "A folder with that name already exists.";
            return false;
        }
        if (ec)
        {
            m_FeedbackIsError = true;
            m_FeedbackMessage = "Unable to verify the project destination.";
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

        bool requestCreateProject = false;

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
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float maxContentWidth = uiLayer.Scale(1040.0f);
            const float horizontalPadding = std::max(
                uiLayer.Scale(18.0f),
                (availableWidth - maxContentWidth) * 0.5f
            );

            ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(horizontalPadding, uiLayer.Scale(14.0f)));

            // Main content
            ImGui::BeginChild("##LauncherMain", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

            // Branding
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::AccentHover);
            ImGui::TextUnformatted("QUIET");
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, uiLayer.Scale(12.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("PROJECT LAUNCHER");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(7.0f)));

            ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(8.0f)));

            // Page header
            if (ImGui::BeginTable("##LauncherHeader", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, uiLayer.Scale(150.0f));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                ImGui::TextUnformatted("Your projects");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextUnformatted("Continue working or create a new project.");
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);

                ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

                if (QUI::EditorButton("+ New Project", ImVec2(-1.0f, 0.0f)))
                {
                    m_NewProjectName.fill('\0');
                    m_FeedbackMessage.clear();
                    requestCreateProject = true;
                }

                ImGui::PopStyleColor(3);
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(8.0f)));

            // Search
            const float availableSearchWidth = ImGui::GetContentRegionAvail().x;
            const float refreshWidth = uiLayer.Scale(90.0f);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;

            const bool horizontalSearchLayout = availableSearchWidth >= uiLayer.Scale(290.0f);
            const float searchWidth = horizontalSearchLayout
                ? std::max(uiLayer.Scale(1.0f), availableSearchWidth - refreshWidth - spacing)
                : std::max(uiLayer.Scale(1.0f), availableSearchWidth);

            ImGui::SetNextItemWidth(searchWidth);
            ImGui::InputTextWithHint("##ProjectSearch", "Search projects...", m_ProjectSearch.data(), m_ProjectSearch.size());

            if (horizontalSearchLayout)
            {
                ImGui::SameLine();
            }

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

            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(9.0f)));

            // Project filtering
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

            std::vector<std::filesystem::path> recentProjects;
            std::vector<std::filesystem::path> otherProjects;

            for (const auto& projectFile : m_ProjectRegistry.RecentProjectFiles)
            {
                if (isValidProject(projectFile) && matchesSearch(projectFile))
                {
                    recentProjects.push_back(projectFile);
                }
            }

            for (const auto& projectFile : m_DiscoveredProjects)
            {
                if (isValidProject(projectFile) &&
                    matchesSearch(projectFile) &&
                    !isRecentProject(projectFile))
                {
                    otherProjects.push_back(projectFile);
                }
            }

            // Project cards
            const auto drawProjectRow = [&](const std::filesystem::path& projectFile, const bool recent)
            {
                ImGui::PushID(projectFile.generic_string().c_str());

                const bool compact = ImGui::GetContentRegionAvail().x < uiLayer.Scale(450.0f);
                const float cardHeight = uiLayer.Scale(compact ? 112.0f : 72.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, uiLayer.Scale(9.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, uiLayer.Scale(ImVec2(14.0f, 11.0f)));
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, uiLayer.Scale(ImVec2(4.0f, 3.0f)));

                ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
                ImGui::PushStyleColor(ImGuiCol_Border, recent ? QUI::EditorColor::Border : QUI::EditorColor::BorderSoft);

                ImGui::BeginChild("##ProjectCard", ImVec2(0.0f, cardHeight),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

                if (ImGui::BeginTable("##ProjectCardLayout", compact ? 1 : 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
                {
                    if (compact)
                    {
                        ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthStretch);
                    }
                    else
                    {
                        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, uiLayer.Scale(40.0f));
                        ImGui::TableSetupColumn("Information", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, uiLayer.Scale(100.0f));
                    }

                    ImGui::TableNextRow();

                    if (!compact)
                    {
                        ImGui::TableSetColumnIndex(0);

                        const ImVec2 iconMin = ImGui::GetCursorScreenPos();
                        const float iconSize = uiLayer.Scale(40.0f);
                        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);

                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        drawList->AddRectFilled(
                            iconMin, iconMax,
                            ImGui::ColorConvertFloat4ToU32(recent ? QUI::EditorColor::AccentSoft : QUI::EditorColor::Background3),
                            uiLayer.Scale(7.0f)
                        );

                        const ImVec2 glyphSize = ImGui::CalcTextSize("Q");

                        drawList->AddText(
                            ImVec2(iconMin.x + (iconSize - glyphSize.x) * 0.5f, iconMin.y + (iconSize - glyphSize.y) * 0.5f),
                            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentHover),
                            "Q"
                        );

                        ImGui::Dummy(ImVec2(iconSize, iconSize));

                        ImGui::TableSetColumnIndex(1);
                    }

                    const std::string projectName = projectFile.stem().string();
                    const std::string projectFolder = projectFile.parent_path().string();

                    // Clip long project names and paths.
                    const ImVec2 textMin = ImGui::GetCursorScreenPos();
                    const float textWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);

                    ImGui::PushClipRect(
                        textMin,
                        ImVec2(textMin.x + textWidth, textMin.y + uiLayer.Scale(80.0f)),
                        true
                    );

                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                    ImGui::TextUnformatted(projectName.c_str());
                    ImGui::PopStyleColor();

                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                    ImGui::TextUnformatted(projectFolder.c_str());
                    ImGui::PopStyleColor();

                    ImGui::PopClipRect();

                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    {
                        ImGui::SetTooltip("%s", projectFile.string().c_str());
                    }

                    if (compact)
                    {
                        ImGui::TableNextRow();
                    }
                    else
                    {
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(7.0f)));
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

                    if (QUI::EditorButton("Open", ImVec2(compact ? -1.0f : uiLayer.Scale(94.0f), 0.0f)))
                    {
                        LaunchEditor(projectFile);
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::EndTable();
                }

                // Project context menu
                if (ImGui::BeginPopupContextWindow("##ProjectCardContext", ImGuiPopupFlags_MouseButtonRight))
                {
                    if (QUI::EditorMenuItem("Open Project"))
                    {
                        LaunchEditor(projectFile);
                    }
                    if (QUI::EditorMenuItem("Show in File Explorer"))
                    {
                        Platform::RevealPathInFileManager(projectFile);
                    }
                    if (QUI::EditorMenuItem("Copy Project Path"))
                    {
                        ImGui::SetClipboardText(projectFile.string().c_str());
                    }

                    ImGui::EndPopup();
                }

                if (hovered)
                {
                    const ImVec2 cardMin = ImGui::GetWindowPos();
                    const ImVec2 cardSize = ImGui::GetWindowSize();

                    ImGui::GetWindowDrawList()->AddRect(
                        ImVec2(cardMin.x + 1.0f, cardMin.y + 1.0f),
                        ImVec2(cardMin.x + cardSize.x - 1.0f, cardMin.y + cardSize.y - 1.0f),
                        ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentSoft),
                        uiLayer.Scale(9.0f)
                    );
                }

                ImGui::EndChild();

                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(3);

                ImGui::PopID();
                ImGui::Spacing();
            };

            // Project library
            const bool hasVisibleProjects = !recentProjects.empty() || !otherProjects.empty();
            if (!hasVisibleProjects)
            {
                ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(20.0f)));

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                ImGui::TextUnformatted(search.empty() ? "No Quiet projects found" : "No matching projects");
                ImGui::PopStyleColor();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextWrapped("%s", search.empty() ? "Create your first project using New Project." : "Try a different project name or search term.");
                ImGui::PopStyleColor();
            }
            else
            {
                if (!recentProjects.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                    ImGui::Text("Recent projects  (%zu)", recentProjects.size());
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    for (const auto& projectFile : recentProjects)
                    {
                        drawProjectRow(projectFile, true);
                    }
                }

                if (!otherProjects.empty())
                {
                    if (!recentProjects.empty())
                    {
                        ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(8.0f)));
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                    ImGui::Text("Other projects  (%zu)", otherProjects.size());
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    for (const auto& projectFile : otherProjects)
                    {
                        drawProjectRow(projectFile, false);
                    }
                }
            }

            // Projects location footer
            ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(12.0f)));

            ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("PROJECTS LOCATION");
            ImGui::PopStyleColor();

            if (QUI::EditorButton("Open Projects Folder"))
            {
                Platform::OpenPathWithDefaultApplication(m_ProjectRegistry.ProjectsRoot);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", m_ProjectRegistry.ProjectsRoot.string().c_str());
            ImGui::PopStyleColor();

            ImGui::EndChild();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Create Project modal
            // Both OpenPopup and BeginPopupModal share the root window's ID scope.
            if (requestCreateProject)
            {
                ImGui::OpenPopup("Create Project");
            }

            const float modalWidth = std::min(
                uiLayer.Scale(460.0f),
                std::max(
                    uiLayer.Scale(240.0f),
                    viewport->WorkSize.x - uiLayer.Scale(32.0f)
                )
            );

            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, uiLayer.Scale(ImVec2(20.0f, 18.0f)));

            if (ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                // Modal header
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                ImGui::TextUnformatted("Create a new project");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextWrapped("Choose a name and start building with QUIET.");
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(12.0f)));

                // Project name
                ImGui::TextUnformatted("Project name");
                ImGui::SetNextItemWidth(-1.0f);

                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                }

                const bool submitted = ImGui::InputTextWithHint("##NewProjectName", "MyProject", m_NewProjectName.data(), m_NewProjectName.size(), ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::IsItemEdited())
                {
                    m_FeedbackMessage.clear();
                }

                const std::string projectName = TrimCopy(m_NewProjectName.data());
                const bool validName = IsValidProjectName(projectName);
                const std::filesystem::path targetFolder = m_ProjectRegistry.ProjectsRoot / projectName;

                std::error_code destinationError;

                const bool folderExists = validName && std::filesystem::exists(targetFolder, destinationError);
                const bool canCreate = validName && !folderExists && !destinationError;

                // Inline validation
                const char* validationMessage = nullptr;
                if (!projectName.empty() && !validName)
                {
                    validationMessage = "Invalid name. Avoid / \\ : * ? \" < > | and reserved names . or ..";
                }
                else if (folderExists)
                {
                    validationMessage = "A folder with this name already exists.";
                }
                else if (destinationError)
                {
                    validationMessage = "Unable to verify the destination folder.";
                }

                if (validationMessage)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);
                    ImGui::TextWrapped("%s", validationMessage);
                    ImGui::PopStyleColor();
                }

                if (m_FeedbackIsError && !m_FeedbackMessage.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);
                    ImGui::TextWrapped("%s", m_FeedbackMessage.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(12.0f)));

                // Project destination
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
                ImGui::TextUnformatted("PROJECT LOCATION");
                ImGui::PopStyleColor();

                const std::string destinationText = (validName ? targetFolder : m_ProjectRegistry.ProjectsRoot).lexically_normal().string();
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextWrapped("%s", destinationText.c_str());
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(16.0f)));

                ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
                ImGui::Separator();
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, uiLayer.Scale(6.0f)));

                // Modal actions
                const float cancelWidth = uiLayer.Scale(86.0f);
                const float createWidth = uiLayer.Scale(156.0f);
                const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
                const float available = ImGui::GetContentRegionAvail().x;

                const bool horizontal = available >= cancelWidth + createWidth + buttonSpacing;
                if (horizontal)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - cancelWidth - createWidth - buttonSpacing);
                }

                const bool cancelPressed = QUI::EditorButton("Cancel", ImVec2(horizontal ? cancelWidth : -1.0f, 0.0f));
                if (horizontal)
                {
                    ImGui::SameLine();
                }

                ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::AccentDim);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::AccentSoft);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Accent);

                const bool createPressed = QUI::EditorButton("Create and Open", ImVec2(horizontal ? createWidth : -1.0f, 0.0f), canCreate);

                ImGui::PopStyleColor(3);

                if (cancelPressed || ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    ImGui::CloseCurrentPopup();
                }
                else if ((createPressed || submitted) && canCreate)
                {
                    std::filesystem::path projectFile;

                    if (CreateProject(projectName, projectFile))
                    {
                        ImGui::CloseCurrentPopup();
                        LaunchEditor(projectFile);
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleVar();
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

}