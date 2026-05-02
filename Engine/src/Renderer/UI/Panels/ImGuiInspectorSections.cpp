#include "ImGuiInspectorSections.h"

#include <cstdio>
#include <imgui.h>

#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI
{
    void TrackedInspectorInteraction::Begin() const
    {
        if (Enabled && ImGui::IsItemActivated())
            Commands.BeginInteraction(SceneRef);
    }

    void TrackedInspectorInteraction::End() const
    {
        if (Enabled && ImGui::IsItemDeactivated())
            Commands.EndInteraction(SceneRef);
    }

    void DrawActorDetailsSection(Scene::Scene& scene,
                                 Scene::Actor& actor,
                                 Editor::EditorCommands& commands,
                                 const TrackedInspectorInteraction& tracked)
    {
        auto& transform = actor.Transform.WorldTransform;
        char nameBuffer[256]{};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

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
    }

    void DrawStaticMeshSection(Scene::Scene& scene,
                               Scene::Actor& actor,
                               Editor::EditorCommands& commands,
                               const Project::ProjectContext& projectContext,
                               const Vulkan::MeshLibrary& meshLibrary)
    {
        const auto meshAssetIDs = projectContext.EnumerateMeshAssetIDs();

        if (!actor.StaticMesh.has_value())
        {
            if (ImGui::Button("Add Static Mesh"))
                commands.AddStaticMesh(scene, actor.ID, projectContext, meshLibrary);

            if (meshAssetIDs.empty())
                ImGui::TextDisabled("No mesh assets registered.");

            return;
        }

        if (!ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        if (meshAssetIDs.empty())
            ImGui::TextDisabled("No mesh assets registered.");
        else
        {
            int currentIndex = 0;
            bool foundCurrent = false;

            for (int i = 0; i < static_cast<int>(meshAssetIDs.size()); ++i)
            {
                if (meshAssetIDs[i] == actor.StaticMesh->MeshResourceID)
                {
                    currentIndex = i;
                    foundCurrent = true;
                    break;
                }
            }

            const char* currentPreview =
                foundCurrent
                    ? meshAssetIDs[currentIndex].c_str()
                    : actor.StaticMesh->MeshResourceID.c_str();

            if (ImGui::BeginCombo("Mesh Resource", currentPreview))
            {
                for (const auto & meshAssetID : meshAssetIDs)
                {
                    const bool isSelected = (meshAssetID == actor.StaticMesh->MeshResourceID);
                    if (ImGui::Selectable(meshAssetID.c_str(), isSelected))
                        commands.SetStaticMeshResource(scene, actor.ID, meshAssetID, projectContext, meshLibrary);
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        bool visible = actor.StaticMesh->Visible;
        if (ImGui::Checkbox("Visible", &visible))
            commands.SetStaticMeshVisible(scene, actor.ID, visible);

        if (ImGui::Button("Remove Static Mesh"))
            commands.RemoveStaticMesh(scene, actor.ID);
    }

    void DrawMaterialSection(Scene::Scene& scene,
                             Scene::Actor& actor,
                             Editor::EditorCommands& commands,
                             const TrackedInspectorInteraction& tracked)
    {
        if (!actor.StaticMesh.has_value()) return;
        if (!actor.Material.has_value())
        {
            if (ImGui::Button("Add Material"))
                commands.AddMaterial(scene, actor.ID);
            return;
        }
        if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) return;

        glm::vec4 baseColor = actor.Material->BaseColor;

        if (ImGui::ColorEdit4("Base Color", &baseColor.x))
            commands.SetMaterialBaseColor(scene, actor.ID, baseColor);

        tracked.Begin();
        tracked.End();

        if (ImGui::Button("Remove Material"))
            commands.RemoveMaterial(scene, actor.ID);
    }

    void DrawRotatorSection(Scene::Scene& scene,
                            Scene::Actor& actor,
                            Editor::EditorCommands& commands,
                            const TrackedInspectorInteraction& tracked)
    {
        if (!actor.Rotator.has_value())
        {
            if (ImGui::Button("Add Rotator"))
                commands.AddRotator(scene, actor.ID);
            return;
        }

        if (!ImGui::CollapsingHeader("Rotator", ImGuiTreeNodeFlags_DefaultOpen))
            return;

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

    void DrawCameraSection(Scene::Scene& scene,
                           Scene::Actor& actor,
                           Editor::EditorCommands& commands,
                           Editor::EditorState& editorState,
                           const TrackedInspectorInteraction& tracked,
                           const bool isEditMode)
    {
        if (!actor.Camera.has_value())
        {
            if (ImGui::Button("Add Camera"))
                commands.AddCamera(scene, actor.ID);
            return;
        }

        if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            return;

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
