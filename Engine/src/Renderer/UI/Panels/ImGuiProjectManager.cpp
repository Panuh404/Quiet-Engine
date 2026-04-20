#include "Quiet/Editor/EditorState.h"

#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Project/ProjectRegistry.h"
#include "Renderer/UI/ImGuiLayer.h"

namespace
{
    void CopyPathToBuffer(char* buffer, std::size_t bufferSize, const std::filesystem::path& path)
    {
        std::snprintf(buffer, bufferSize, "%s", path.string().c_str());
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildProjectManagerPopup(Editor::EditorState &editorState, const Project::ProjectContext &projectContext, const Project::ProjectRegistry &projectRegistry)
    {
        if (editorState.RequestProjectManagerPopup)
        {
            ImGui::OpenPopup("Project Manager");
            editorState.RequestProjectManagerPopup = false;
        }

        if (!ImGui::BeginPopupModal("Project Manager", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("Current Project: %s", projectContext.GetProjectName().c_str());
        ImGui::TextWrapped("%s", projectContext.GetProjectFilePath().string().c_str());
        ImGui::Separator();

        ImGui::Text("Projects Root:");
        ImGui::TextWrapped("%s", projectRegistry.ProjectsRoot.string().c_str());
        ImGui::Separator();

        ImGui::TextUnformatted("Create New Project");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("Project Name",
                         editorState.NewProjectNameBuffer.data(),
                         editorState.NewProjectNameBuffer.size());

        if (ImGui::Button("Create Project", ImVec2(160.0f, 0.0f)))
        {
            editorState.RequestCreateProjectConfirm = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();

        ImGui::TextUnformatted("Recent Projects");
        for (const auto& recentProject : projectRegistry.RecentProjectFiles)
        {
            if (ImGui::Selectable(recentProject.string().c_str()))
            {
                CopyPathToBuffer(
                    editorState.ProjectToOpenBuffer.data(),
                    editorState.ProjectToOpenBuffer.size(),
                    recentProject
                );
                editorState.RequestOpenProjectConfirm = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Separator();

        ImGui::TextUnformatted("Projects In Root");
        const std::vector<std::filesystem::path> discoveredProjects =
            Project::ProjectRegistryIO::EnumerateProjectFiles(projectRegistry.ProjectsRoot);

        ImGui::BeginChild("##ProjectList", ImVec2(520.0f, 220.0f), ImGuiChildFlags_Borders);
        {
            for (const auto& projectFile : discoveredProjects)
            {
                const std::string label = projectFile.string();
                if (ImGui::Selectable(label.c_str()))
                {
                    CopyPathToBuffer(
                        editorState.ProjectToOpenBuffer.data(),
                        editorState.ProjectToOpenBuffer.size(),
                        projectFile
                    );
                    editorState.RequestOpenProjectConfirm = true;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(160.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
