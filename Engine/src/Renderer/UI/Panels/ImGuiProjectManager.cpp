#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Project/ProjectRegistry.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace QUI = Quiet::Renderer::UI;
namespace SZ = Quiet::Renderer::UI::EditorSizing;

namespace
{
    void CopyPathToBuffer(char* buffer, std::size_t bufferSize, const std::filesystem::path& path)
    {
        std::snprintf(buffer, bufferSize, "%s", path.generic_string().c_str());
    }

    bool BufferHasText(const char* buffer)
    {
        return buffer != nullptr && buffer[0] != '\0';
    }

    bool IsSelectedPath(const char* buffer, const std::filesystem::path& path)
    {
        return std::string(buffer) == path.generic_string();
    }

    void DrawSectionHeader(const char* title, const char* subtitle = nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        if (subtitle && subtitle[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        ImGui::Spacing();
    }

    void DrawMetaRow(const char* label, const std::string& value, const float valueColumnX)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(valueColumnX);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopStyleColor();
    }

    void DrawStatChip(const char* label, const std::string& value)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background3);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild(label, ImVec2(0.0f, SZ::Px(58.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", value.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    bool PassFilter(const ImGuiTextFilter& filter, const std::filesystem::path& path)
    {
        return filter.PassFilter(path.generic_string().c_str());
    }

    void DrawSearchFilter(ImGuiTextFilter& filter, const char* label, const char* hint)
    {
        ImGui::PushID(label);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-1.0f);

        const std::string inputLabel = std::string("##") + label;
        filter.Draw(inputLabel.c_str(), -1.0f);

        if (!filter.InputBuf[0] && !ImGui::IsItemActive())
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 textPos = ImVec2(
                min.x + ImGui::GetStyle().FramePadding.x,
                min.y + ImGui::GetStyle().FramePadding.y
            );

            ImGui::GetWindowDrawList()->AddText(
                textPos,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextDim),
                hint
            );
        }

        ImGui::PopID();
    }

    bool DrawListRow(const std::string& title, const std::string& subtitle, bool selected, bool emphasize = false)
    {
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, QUI::EditorColor::AccentSoft);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, QUI::EditorColor::Accent);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, QUI::EditorColor::AccentHover);
        }
        else if (emphasize)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.110f, 0.090f, 0.030f, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.155f, 0.125f, 0.040f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.190f, 0.155f, 0.050f, 1.00f));
        }

        const bool clicked = QUI::EditorSelectable(
            title.c_str(),
            selected,
            ImGuiSelectableFlags_AllowOverlap
        );

        if (selected || emphasize)
        {
            ImGui::PopStyleColor(3);
        }

        if (!subtitle.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", subtitle.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        return clicked;
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildProjectManagerPopup(Editor::EditorState& editorState,
                                              const Project::ProjectContext& projectContext,
                                              const Project::ProjectRegistry& projectRegistry)
    {
        static ImGuiTextFilter sceneFilter;
        static ImGuiTextFilter projectFilter;

        if (editorState.ProjectManager.RequestOpenPopup)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 targetSize = Scale(ImVec2(1200.0f, 940.0f));
            const ImVec2 maxSize = ImVec2(
                viewport->WorkSize.x * 0.96f,
                viewport->WorkSize.y * 0.94f
            );

            const ImVec2 popupSize = ImVec2(
                std::min(targetSize.x, maxSize.x),
                std::min(targetSize.y, maxSize.y)
            );

            ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(
                viewport->GetCenter(),
                ImGuiCond_Appearing,
                ImVec2(0.5f, 0.5f)
            );

            ImGui::OpenPopup("Project Manager");
            editorState.ProjectManager.RequestOpenPopup = false;
        }

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        bool projectManagerOpen = true;
        if (!QUI::EditorBeginPopupModal("Project Manager", projectManagerOpen, windowFlags))
            return;

        const auto& descriptor = projectContext.GetProjectDescriptor();
        const std::vector<std::filesystem::path> scenes = projectContext.EnumerateSceneFiles();
        const std::vector<std::filesystem::path> discoveredProjects =
            Project::ProjectRegistryIO::EnumerateProjectFiles(projectRegistry.ProjectsRoot);

        const bool hasSelectedScene = BufferHasText(editorState.ProjectManager.ProjectScenePathBuffer.data());
        const std::filesystem::path startupSceneRelative =
            projectContext.MakeSceneRelativePath(projectContext.GetStartupScenePath());

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Scale(ImVec2(10.0f, 8.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Scale(ImVec2(10.0f, 7.0f)));

        DrawSectionHeader(
            "Project Manager",
            "Manage Quiet projects, switch scenes, and control the project startup scene."
        );

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Scale(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##ProjectSummaryCard", Scale(ImVec2(0.0f, 190.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(projectContext.GetProjectName().c_str());
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", projectContext.GetProjectFilePath().string().c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            if (ImGui::BeginTable("##ProjectStats", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                DrawStatChip("Scenes", std::to_string(scenes.size()));

                ImGui::TableSetColumnIndex(1);
                DrawStatChip("Recent", std::to_string(projectRegistry.RecentProjectFiles.size()));

                ImGui::TableSetColumnIndex(2);
                DrawStatChip("Startup", descriptor.StartupScene);

                ImGui::EndTable();
            }

            ImGui::Spacing();

            const float metaColumnX = Scale(135.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Created");
            ImGui::PopStyleColor();

            ImGui::SameLine(metaColumnX);

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(descriptor.CreatedAtUtc.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine(Scale(410.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Updated");
            ImGui::PopStyleColor();

            ImGui::SameLine(Scale(485.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(descriptor.UpdatedAtUtc.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();

        const float bodyHeight = std::max(Scale(320.0f), ImGui::GetContentRegionAvail().y - Scale(8.0f));

        if (ImGui::BeginTable("##ProjectManagerLayout", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.58f);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                ImGui::BeginChild("##ProjectManagerLeft", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None);
                {
                    DrawSectionHeader("Create New Project");

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText(
                        "##ProjectName",
                        editorState.ProjectManager.NewProjectNameBuffer.data(),
                        editorState.ProjectManager.NewProjectNameBuffer.size()
                    );
                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                    ImGui::TextUnformatted("A new folder and .quiet file will be created under the projects root.");
                    ImGui::PopStyleColor();

                    const bool canCreateProject = BufferHasText(editorState.ProjectManager.NewProjectNameBuffer.data());
                    if (EditorButton("Create Project", ImVec2(-1.0f, 0.0f), canCreateProject))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::CreateProject,
                            editorState.ProjectManager.NewProjectNameBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    DrawSectionHeader("Recent Projects");

                    ImGui::BeginChild("##RecentProjects", Scale(ImVec2(0.0f, 130.0f)), ImGuiChildFlags_Borders);
                    {
                        if (projectRegistry.RecentProjectFiles.empty())
                        {
                            QUI::DrawMutedText("No recent projects yet.");
                        }
                        else
                        {
                            for (const auto& recentProject : projectRegistry.RecentProjectFiles)
                            {
                                const std::string title = recentProject.stem().string() + "##" + recentProject.string();
                                if (DrawListRow(title, recentProject.string(), false))
                                {
                                    CopyPathToBuffer(
                                        editorState.ProjectManager.ProjectToOpenBuffer.data(),
                                        editorState.ProjectManager.ProjectToOpenBuffer.size(),
                                        recentProject
                                    );
                                    editorState.QueueRequest(
                                        Editor::EditorRequestType::OpenProject,
                                        editorState.ProjectManager.ProjectToOpenBuffer.data()
                                    );
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }
                    }
                    ImGui::EndChild();

                    ImGui::Spacing();

                    DrawSectionHeader("Projects In Root", projectRegistry.ProjectsRoot.string().c_str());
                    DrawSearchFilter(projectFilter, "Projects Search", "Search projects...");

                    ImGui::BeginChild("##DiscoveredProjects", Scale(ImVec2(0.0f, 150.0f)), ImGuiChildFlags_Borders);
                    {
                        bool anyShown = false;
                        for (const auto& projectFile : discoveredProjects)
                        {
                            if (!PassFilter(projectFilter, projectFile))
                            {
                                continue;
                            }

                            anyShown = true;
                            const std::string title = projectFile.stem().string() + "##" + projectFile.string();
                            if (DrawListRow(title, projectFile.string(), false))
                            {
                                CopyPathToBuffer(
                                    editorState.ProjectManager.ProjectToOpenBuffer.data(),
                                    editorState.ProjectManager.ProjectToOpenBuffer.size(),
                                    projectFile
                                );

                                editorState.QueueRequest(
                                    Editor::EditorRequestType::OpenProject,
                                    editorState.ProjectManager.ProjectToOpenBuffer.data()
                                );

                                ImGui::CloseCurrentPopup();
                            }
                        }

                        if (!anyShown)
                        {
                            QUI::DrawMutedText("No matching .quiet projects found.");
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }

            ImGui::TableSetColumnIndex(1);
            {
                ImGui::BeginChild("##ProjectManagerRight", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None);
                {
                    DrawSectionHeader("Project Scenes", "Select a scene to open it or mark it as the project startup scene.");
                    DrawSearchFilter(sceneFilter, "Scenes Search", "Search scenes...");

                    const float sceneActionsHeight = Scale(170.0f);
                    const float sceneListHeight = std::max(
                        Scale(180.0f),
                        ImGui::GetContentRegionAvail().y - sceneActionsHeight
                    );

                    ImGui::BeginChild("##ProjectScenes", ImVec2(0.0f, sceneListHeight), ImGuiChildFlags_Borders);
                    {
                        bool anyShown = false;
                        for (const auto& scenePath : scenes)
                        {
                            if (!PassFilter(sceneFilter, scenePath))
                            {
                                continue;
                            }

                            anyShown = true;
                            const bool isStartup = (startupSceneRelative == scenePath);
                            const bool isSelected =
                                IsSelectedPath(editorState.ProjectManager.ProjectScenePathBuffer.data(), scenePath);

                            std::string title = scenePath.generic_string();
                            if (isStartup)
                            {
                                title += "  [Startup]";
                            }

                            if (DrawListRow(title, "", isSelected, isStartup && !isSelected))
                            {
                                CopyPathToBuffer(
                                    editorState.ProjectManager.ProjectScenePathBuffer.data(),
                                    editorState.ProjectManager.ProjectScenePathBuffer.size(),
                                    scenePath
                                );
                            }
                        }

                        if (!anyShown)
                        {
                            QUI::DrawMutedText("No matching scene files found.");
                        }
                    }
                    ImGui::EndChild();

                    ImGui::Spacing();

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Scale(8.0f));
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
                    ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);
                    ImGui::BeginChild("##SelectedSceneCard", Scale(ImVec2(0.0f, 92.0f)), ImGuiChildFlags_Borders);
                    {
                        if (hasSelectedScene)
                        {
                            ImGui::TextUnformatted("Selected Scene");
                            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                            ImGui::TextUnformatted(editorState.ProjectManager.ProjectScenePathBuffer.data());

                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            {
                                ImGui::SetTooltip("%s", editorState.ProjectManager.ProjectScenePathBuffer.data());
                            }
                            ImGui::PopStyleColor();

                            ImGui::Spacing();

                            if (EditorButton("Copy Path", Scale(ImVec2(120.0f, 0.0f))))
                            {
                                ImGui::SetClipboardText(editorState.ProjectManager.ProjectScenePathBuffer.data());
                            }

                            ImGui::SameLine();

                            if (EditorButton("Clear Selection", Scale(ImVec2(140.0f, 0.0f))))
                            {
                                editorState.ProjectManager.ProjectScenePathBuffer[0] = '\0';
                            }
                        }
                        else
                        {
                            QUI::DrawMutedText("No scene selected.");
                            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                            ImGui::TextWrapped("Select a scene from the list to enable scene actions.");
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();

                    ImGui::Spacing();

                    if (EditorButton("Open Scene", Scale(ImVec2(150.0f, 0.0f)), hasSelectedScene))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::OpenProjectScene,
                            editorState.ProjectManager.ProjectScenePathBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();

                    if (EditorButton("Set As Startup", Scale(ImVec2(170.0f, 0.0f)), hasSelectedScene))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::SetStartupScene,
                            editorState.ProjectManager.ProjectScenePathBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
}