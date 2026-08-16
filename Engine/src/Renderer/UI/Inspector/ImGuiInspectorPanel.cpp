#include "Renderer/UI/Inspector/ImGuiInspectorPanel.h"

#include <imgui.h>

#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/Inspector/ImGuiInspectorSections.h"
#include "Renderer/UI/Inspector/ImGuiInspectorWidgets.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Inspector
{
    void DrawInspectorPanel(InspectorContext& context, InspectorState& state)
    {
        ImGui::Begin("Inspector");

        Scene::Actor* selectedActor = nullptr;
        if (context.EditorState.Selection.SelectedActorID.has_value())
        {
            selectedActor = context.Scene.FindActorByID(context.EditorState.Selection.SelectedActorID.value());
            if (!selectedActor)
            {
                context.EditorState.Selection.SelectedActorID.reset();
            }
        }

        const bool isEditMode = context.EditorState.Scene.Mode == Editor::SceneMode::Edit;
        const TrackedInspectorInteraction tracked{ context.Scene, context.Commands, isEditMode };

        if (!isEditMode)
        {
            DrawInspectorBanner("Simulating runtime copy. Editing is disabled.");
            ImGui::Spacing();
        }

        if (!isEditMode)
        {
            ImGui::BeginDisabled();
        }

        if (!selectedActor)
        {
            DrawInspectorEmptyState();
            if (!isEditMode)
            {
                ImGui::EndDisabled();
            }

            ImGui::End();
            return;
        }

        Scene::Actor& actor = *selectedActor;
        ImGui::Spacing();

        DrawActorDetailsSection(context, state, actor, tracked);
        ImGui::Spacing();

        DrawSectionTitle("Components");
        DrawAddComponentMenu(context, actor);

        ImGui::Spacing();

        DrawStaticMeshSection(context, state, actor);
        DrawMaterialSection(context, state, actor, tracked);
        DrawRigidBodySection(context, state, actor, tracked);
        DrawBoxColliderSection(context, state, actor, tracked);
        DrawDirectionalLightSection(context, state, actor, tracked);
        DrawRotatorSection(context, state, actor, tracked);
        DrawCameraSection(context, state, actor, tracked, isEditMode);

        if (!isEditMode)
        {
            ImGui::EndDisabled();
        }

        ImGui::End();
    }
}