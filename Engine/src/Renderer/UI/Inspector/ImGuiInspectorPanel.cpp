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

        const bool isEditMode = context.EditorState.Scene.Mode == Editor::SceneMode::Edit;
        auto& selection = context.EditorState.Selection;

        Scene::Actor* selectedActor = nullptr;
        if (const auto actorID = selection.GetPrimaryActorID())
        {
            selectedActor = context.Scene.FindActorByID(*actorID);
            if (!selectedActor && isEditMode)
            {
                selection.RemoveActor(*actorID);
                if (const auto nextActorID = selection.GetPrimaryActorID())
                    selectedActor = context.Scene.FindActorByID(*nextActorID);
            }
        }

        if (selection.HasMultipleActors())
        {
            ImGui::Text("%zu actors selected", selection.GetActorCount());
            if (selectedActor)
            {
                const auto& name = selectedActor->GetName();
                ImGui::Text("Primary actor: %s", name.empty() ? "Unnamed Actor" : name.c_str());
            }
            DrawInspectorBanner("Select one actor to edit its properties.");
            ImGui::End();
            return;
        }

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
        DrawSphereColliderSection(context, state, actor, tracked);
        DrawCapsuleColliderSection(context, state, actor, tracked);

        DrawDirectionalLightSection(context, state, actor, tracked);
        DrawRotatorSection(context, state, actor, tracked);
        DrawTextSection(context, state, actor, tracked);
        DrawUIRectSection(context, state, actor, tracked);
        DrawCameraSection(context, state, actor, tracked, isEditMode);
        DrawAudioSourceSection(context, state, actor, tracked);
        DrawAudioListenerSection(context, state, actor);
        DrawScriptSection(context, state, actor, tracked);

        state.SectionVisibilityRequest = InspectorSectionVisibilityRequest::None;

        if (!isEditMode)
        {
            ImGui::EndDisabled();
        }

        ImGui::End();
    }
}
