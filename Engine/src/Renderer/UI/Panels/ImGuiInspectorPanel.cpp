#include "ImGuiInspectorSections.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;

    void DrawInspectorBanner(const char* message)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::AccentSoft);

        ImGui::BeginChild("##InspectorBanner", ImVec2(0.0f, 44.0f), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", message);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawInspectorEmptyState()
    {
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##InspectorEmptyState", ImVec2(0.0f, 92.0f), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No actor selected");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Select an actor from the Hierarchy or the viewport to inspect its components.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildInspector(Scene::Scene& scene,
                                    Editor::EditorCommands& commands,
                                    Editor::EditorState& editorState,
                                    const Project::ProjectContext& projectContext,
                                    const Vulkan::MeshLibrary& meshLibrary,
                                    const Vulkan::TextureLibrary& textureLibrary)
    {
        ImGui::Begin("Inspector");
        {
            Scene::Actor* selectedActor = nullptr;
            if (editorState.Selection.SelectedActorID.has_value())
            {
                selectedActor = scene.FindActorByID(editorState.Selection.SelectedActorID.value());
                if (!selectedActor) editorState.Selection.SelectedActorID.reset();
            }

            const bool isEditMode = (editorState.Scene.Mode == Editor::SceneMode::Edit);
            const TrackedInspectorInteraction tracked{ scene, commands, isEditMode };

            if (!isEditMode)
            {
                DrawInspectorBanner("Simulating runtime copy. Editing is disabled.");
                ImGui::Spacing();
            }

            if (!isEditMode) ImGui::BeginDisabled();

            if (!selectedActor)
            {
                DrawInspectorEmptyState();
                if (!isEditMode) ImGui::EndDisabled();

                ImGui::End();
                return;
            }

            ImGui::Spacing();
            auto& actor = *selectedActor;
            DrawActorDetailsSection(scene, actor, commands, tracked);

            ImGui::Spacing();
            DrawSectionTitle("Components");

            DrawAddComponentMenu(scene, actor, commands, projectContext, meshLibrary);

            ImGui::Spacing();

            DrawStaticMeshSection(scene, actor, commands, projectContext, meshLibrary);
            DrawMaterialSection(scene, actor, commands, projectContext, textureLibrary, tracked, m_UIScale);
            DrawDirectionalLightSection(scene, actor, commands, tracked);
            DrawRotatorSection(scene, actor, commands, tracked);
            DrawCameraSection(scene, actor, commands, editorState, tracked, isEditMode);

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }
}
