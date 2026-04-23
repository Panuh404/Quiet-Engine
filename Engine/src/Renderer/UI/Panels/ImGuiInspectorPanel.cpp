#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    struct TrackedInspectorInteraction
    {
        Quiet::Scene::Scene& Scene;
        Quiet::Editor::EditorCommands& Commands;
        bool Enabled = false;

        void Begin() const
        {
            if (Enabled && ImGui::IsItemActivated())
                Commands.BeginInteraction(Scene);
        }

        void End() const
        {
            if (Enabled && ImGui::IsItemDeactivated())
                Commands.EndInteraction(Scene);
        }
    };

    void DrawStaticMeshSection(Quiet::Scene::Scene& scene,
                               Quiet::Scene::Actor& actor,
                               Quiet::Editor::EditorCommands& commands,
                               const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (!actor.StaticMesh.has_value())
        {
            if (ImGui::Button("Add Static Mesh"))
                commands.AddStaticMesh(scene, actor.ID, meshLibrary);
            return;
        }

        if (!ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen)) return;

        const auto names = meshLibrary.GetNames();

        int currentIndex = 0;
        for (int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            if (names[i] == actor.StaticMesh->MeshResourceID)
            {
                currentIndex = i;
                break;
            }
        }

        const char* currentPreview = names.empty() ? "<none>" : names[currentIndex].c_str();

        if (ImGui::BeginCombo("Mesh Resource", currentPreview))
        {
            for (int i = 0; i < static_cast<int>(names.size()); ++i)
            {
                const bool isSelected = (i == currentIndex);
                if (ImGui::Selectable(names[i].c_str(), isSelected))
                    commands.SetStaticMeshResource(scene, actor.ID, names[i], meshLibrary);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        bool visible = actor.StaticMesh->Visible;
        if (ImGui::Checkbox("Visible", &visible))
            commands.SetStaticMeshVisible(scene, actor.ID, visible);

        if (ImGui::Button("Remove Static Mesh"))
            commands.RemoveStaticMesh(scene, actor.ID);
    }

    void DrawRotatorSection(Quiet::Scene::Scene& scene,
                            Quiet::Scene::Actor& actor,
                            Quiet::Editor::EditorCommands& commands,
                            const TrackedInspectorInteraction& tracked)
    {
        if (!actor.Rotator.has_value())
        {
            if (ImGui::Button("Add Rotator"))
                commands.AddRotator(scene, actor.ID);
            return;
        }

        if (!ImGui::CollapsingHeader("Rotator", ImGuiTreeNodeFlags_DefaultOpen)) return;

        if (ImGui::DragFloat("Rotation Speed",
                             &actor.Rotator->RotationSpeedDegreesPerSecond,
                             1.0f))
        {
            commands.SetRotatorSpeed(
                scene,
                actor.ID,
                actor.Rotator->RotationSpeedDegreesPerSecond
            );
        }
        tracked.Begin();
        tracked.End();

        if (ImGui::Button("Remove Rotator"))
            commands.RemoveRotator(scene, actor.ID);
    }

    void DrawCameraSection(Quiet::Scene::Scene& scene,
                           Quiet::Scene::Actor& actor,
                           Quiet::Editor::EditorCommands& commands,
                           Quiet::Editor::EditorState& editorState,
                           const TrackedInspectorInteraction& tracked,
                           const bool isEditMode)
    {
        if (!actor.Camera.has_value())
        {
            if (ImGui::Button("Add Camera"))
                commands.AddCamera(scene, actor.ID);
            return;
        }

        if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) return;

        const bool isPilotingThisCamera =
            editorState.Selection.PilotedCameraActorID.has_value() &&
            editorState.Selection.PilotedCameraActorID.value() == actor.ID;

        if (isEditMode)
        {
            if (!isPilotingThisCamera)
            {
                if (ImGui::Button("Pilot Camera"))
                    editorState.Selection.PilotedCameraActorID = actor.ID;
            }
            else
            {
                if (ImGui::Button("Stop Piloting"))
                    editorState.Selection.PilotedCameraActorID.reset();
            }
        }

        bool primary = actor.Camera->Primary;
        if (ImGui::Checkbox("Primary", &primary))
            commands.SetCameraPrimary(scene, actor.ID, primary);

        float fov = actor.Camera->FieldOfViewDegrees;
        if (ImGui::DragFloat("Field Of View", &fov, 0.25f, 1.0f, 179.0f))
            commands.SetCameraFieldOfView(scene, actor.ID, fov);

        tracked.Begin();
        tracked.End();

        float nearClip = actor.Camera->NearClip;
        if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, 100.0f))
            commands.SetCameraNearClip(scene, actor.ID, nearClip);

        tracked.Begin();
        tracked.End();

        float farClip = actor.Camera->FarClip;
        if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 1.0f, 100000.0f))
            commands.SetCameraFarClip(scene, actor.ID, farClip);

        tracked.Begin();
        tracked.End();

        if (ImGui::Button("Remove Camera"))
            commands.RemoveCamera(scene, actor.ID);
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildInspector(Scene::Scene &scene,
                                    Editor::EditorCommands &commands,
                                    Editor::EditorState &editorState,
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
            auto& transform = actor.Transform.WorldTransform;

            const TrackedInspectorInteraction tracked{ scene, commands, isEditMode };

            char nameBuffer[256]{};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

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

            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                commands.RenameActor(scene, actor.ID, nameBuffer);

            tracked.Begin();
            tracked.End();

            if (ImGui::DragFloat3("Position", &transform.Position.x, 0.1f))
                commands.SetActorPosition(scene, actor.ID, transform.Position);

            tracked.Begin();
            tracked.End();

            if (ImGui::DragFloat3("Rotation", &transform.RotationDegrees.x, 0.5f))
                commands.SetActorRotation(scene, actor.ID, transform.RotationDegrees);

            tracked.Begin();
            tracked.End();

            if (ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.01f, 100.0f))
                commands.SetActorScale(scene, actor.ID, transform.Scale);

            tracked.Begin();
            tracked.End();

            ImGui::Separator();
            ImGui::Text("Components");

            DrawStaticMeshSection(scene, actor, commands, meshLibrary);
            DrawRotatorSection(scene, actor, commands, tracked);
            DrawCameraSection(scene, actor, commands, editorState, tracked, isEditMode);

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }
}
