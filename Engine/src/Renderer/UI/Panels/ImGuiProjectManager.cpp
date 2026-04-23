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
        ImGui::TextUnformatted(title);
        if (subtitle && subtitle[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    void DrawMetaRow(const char* label, const std::string& value, const float valueColumnX)
    {
        ImGui::Text("%s", label);
        ImGui::SameLine(valueColumnX);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.88f, 0.91f, 1.0f));
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopStyleColor();
    }

    void DrawStatChip(const char* label, const std::string& value)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.17f, 1.0f));
        ImGui::BeginChild(label, ImVec2(0.0f, 86.0f), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();

            ImGui::TextUnformatted(value.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    bool PassFilter(const ImGuiTextFilter& filter, const std::filesystem::path& path)
    {
        return filter.PassFilter(path.generic_string().c_str());
    }

    bool DrawListRow(const std::string& title,
                     const std::string& subtitle,
                     bool selected,
                     bool emphasize = false)
    {
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.33f, 0.56f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.23f, 0.37f, 0.62f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.29f, 0.50f, 1.0f));
        }
        else if (emphasize)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.19f, 0.22f, 0.16f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.28f, 0.18f, 0.70f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.30f, 0.19f, 0.85f));
        }

        const bool clicked = ImGui::Selectable(title.c_str(), selected, ImGuiSelectableFlags_AllowOverlap);

        if (selected || emphasize)
        {
            ImGui::PopStyleColor(3);
        }

        if (!subtitle.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
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
            ImGui::SetNextWindowSize(Scale(ImVec2(1040.0f, 900.0f)), ImGuiCond_Appearing);
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
            ImGuiWindowFlags_NoSavedSettings;

        if (!ImGui::BeginPopupModal("Project Manager", nullptr, windowFlags))
        {
            return;
        }

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
            "Manage Quiet projects, switch scenes, and control the project startup scene."
        );

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
        ImGui::BeginChild("##ProjectSummaryCard", Scale(ImVec2(0.0f, 280.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::TextUnformatted(projectContext.GetProjectName().c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
            ImGui::TextWrapped("%s", projectContext.GetProjectFilePath().string().c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            if (ImGui::BeginTable("##ProjectStats", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                DrawStatChip("Scenes", std::to_string(scenes.size()));

                ImGui::TableSetColumnIndex(1);
                DrawStatChip("Recent Projects", std::to_string(projectRegistry.RecentProjectFiles.size()));

                ImGui::TableSetColumnIndex(2);
                DrawStatChip("Startup Scene", descriptor.StartupScene);

                ImGui::EndTable();
            }

            ImGui::Spacing();
            DrawMetaRow("Created", descriptor.CreatedAtUtc, Scale(165.0f));
            DrawMetaRow("Last Updated", descriptor.UpdatedAtUtc, Scale(165.0f));
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::Spacing();

        if (ImGui::BeginTable("##ProjectManagerLayout", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.58f);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                ImGui::BeginChild("##ProjectManagerLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                {
                    DrawSectionHeader("Create New Project");

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText(
                        "##ProjectName",
                        editorState.ProjectManager.NewProjectNameBuffer.data(),
                        editorState.ProjectManager.NewProjectNameBuffer.size()
                    );
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
                    ImGui::TextUnformatted("A new folder and .quiet file will be created under the projects root.");
                    ImGui::PopStyleColor();

                    const bool canCreateProject = BufferHasText(editorState.ProjectManager.NewProjectNameBuffer.data());
                    if (!canCreateProject)
                    {
                        ImGui::BeginDisabled();
                    }

                    if (ImGui::Button("Create Project", ImVec2(-1.0f, 0.0f)))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::CreateProject,
                            editorState.ProjectManager.NewProjectNameBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }

                    if (!canCreateProject)
                    {
                        ImGui::EndDisabled();
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    DrawSectionHeader("Recent Projects");

                    ImGui::BeginChild("##RecentProjects", Scale(ImVec2(0.0f, 180.0f)), ImGuiChildFlags_Borders);
                    {
                        if (projectRegistry.RecentProjectFiles.empty())
                        {
                            ImGui::TextDisabled("No recent projects yet.");
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
                    projectFilter.Draw("Filter Projects", -1.0f);

                    ImGui::BeginChild("##DiscoveredProjects", Scale(ImVec2(0.0f, 215.0f)), ImGuiChildFlags_Borders);
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
                                    Editor::EditorRequestType::OpenProjectScene,
                                    editorState.ProjectManager.ProjectScenePathBuffer.data()
                                );
                                ImGui::CloseCurrentPopup();
                            }
                        }

                        if (!anyShown)
                        {
                            ImGui::TextDisabled("No matching .quiet projects found.");
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }

            ImGui::TableSetColumnIndex(1);
            {
                ImGui::BeginChild("##ProjectManagerRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                {
                    DrawSectionHeader("Project Scenes", "Select a scene to open it or mark it as the project startup scene.");
                    sceneFilter.Draw("Filter Scenes", -1.0f);

                    ImGui::BeginChild("##ProjectScenes", Scale(ImVec2(0.0f, 430.0f)), ImGuiChildFlags_Borders);
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
                            ImGui::TextDisabled("No matching scene files found.");
                        }
                    }
                    ImGui::EndChild();

                    ImGui::Spacing();

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Scale(8.0f));
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
                    ImGui::BeginChild("##SelectedSceneCard", Scale(ImVec2(0.0f, 112.0f)), ImGuiChildFlags_Borders);
                    {
                        if (hasSelectedScene)
                        {
                            ImGui::TextUnformatted("Selected Scene");
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.88f, 0.91f, 1.0f));
                            ImGui::TextWrapped("%s", editorState.ProjectManager.ProjectScenePathBuffer.data());
                            ImGui::PopStyleColor();

                            ImGui::Spacing();

                            if (ImGui::Button("Copy Path", Scale(ImVec2(120.0f, 0.0f))))
                            {
                                ImGui::SetClipboardText(editorState.ProjectManager.ProjectScenePathBuffer.data());
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Clear Selection", Scale(ImVec2(140.0f, 0.0f))))
                            {
                                editorState.ProjectManager.ProjectScenePathBuffer[0] = '\0';
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("No scene selected.");
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
                            ImGui::TextWrapped("Select a scene from the list to enable scene actions.");
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();

                    ImGui::Spacing();

                    if (!hasSelectedScene)
                    {
                        ImGui::BeginDisabled();
                    }

                    if (ImGui::Button("Open Scene", Scale(ImVec2(150.0f, 0.0f))))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::OpenProjectScene,
                            editorState.ProjectManager.ProjectScenePathBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Set As Startup", Scale(ImVec2(170.0f, 0.0f))))
                    {
                        editorState.QueueRequest(
                            Editor::EditorRequestType::SetStartupScene,
                            editorState.ProjectManager.ProjectScenePathBuffer.data()
                        );
                        ImGui::CloseCurrentPopup();
                    }

                    if (!hasSelectedScene)
                    {
                        ImGui::EndDisabled();
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", Scale(ImVec2(140.0f, 0.0f))))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
}