#include "ImGuiInspectorSections.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"


namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildInspector(Scene::Scene &scene,
                                    Editor::EditorCommands &commands,
                                    Editor::EditorState &editorState,
                                    const Project::ProjectContext& projectContext,
                                    const Vulkan::MeshLibrary &meshLibrary)
    {
        ImGui::Begin("Inspector");
        {
            Scene::Actor* selectedActor = nullptr;
            if (editorState.Selection.SelectedActorID.has_value())
            {
                selectedActor = scene.FindActorByID(editorState.Selection.SelectedActorID.value());
                if (!selectedActor)
                    editorState.Selection.SelectedActorID.reset();
            }

            if (!selectedActor)
            {
                ImGui::Text("No actor selected.");
                ImGui::End();
                return;
            }

            const bool isEditMode = (editorState.Scene.Mode == Editor::SceneMode::Edit);
            auto& actor = *selectedActor;

            const TrackedInspectorInteraction tracked{ scene, commands, isEditMode };

            ImGui::Text("Actor");
            ImGui::Separator();

            if (!isEditMode)
            {
                ImGui::TextColored(
                    ImVec4(0.30f, 0.85f, 0.35f, 1.0f),
                    "Simulating runtime copy. Editing is disabled."
                );
                ImGui::Separator();
            }

            if (!isEditMode) ImGui::BeginDisabled();

            DrawActorDetailsSection(scene, actor, commands, tracked);

            ImGui::Separator();
            ImGui::Text("Components");

            DrawStaticMeshSection(scene, actor, commands, projectContext, meshLibrary);
            DrawMaterialSection(scene, actor, commands, tracked);
            DrawRotatorSection(scene, actor, commands, tracked);
            DrawCameraSection(scene, actor, commands, editorState, tracked, isEditMode);

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }
}
