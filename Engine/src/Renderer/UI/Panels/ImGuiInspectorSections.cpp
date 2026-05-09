#include "ImGuiInspectorSections.h"

#include <cstdio>
#include <imgui.h>

#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
    bool HasActiveDirectionalLight(const Quiet::Scene::Scene& scene)
    {
        for (const Quiet::Scene::Actor& actor : scene.GetActors())
        {
            if (actor.DirectionalLight.has_value() && actor.DirectionalLight->Enabled)
                return true;
        }
        return false;
    }
}

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

    void DrawSceneLightingSection(Scene::Scene &scene,
                                  Editor::EditorCommands &commands,
                                  const TrackedInspectorInteraction &tracked)
    {
        if (!ImGui::CollapsingHeader("Scene Lighting", ImGuiTreeNodeFlags_DefaultOpen)) return;
        Scene::SceneRenderSettings& settings = scene.GetRenderSettings();
        const bool hasActiveDirectionalLight = HasActiveDirectionalLight(scene);
        if (hasActiveDirectionalLight)
        {
            ImGui::TextDisabled("Direct light is controlled by the first enabled Directional Light actor.");
            ImGui::TextDisabled("Ambient settings below still apply.");
        }
        else
        {
            ImGui::TextDisabled("Using scene fallback direct light.");
        }

        if (hasActiveDirectionalLight) ImGui::BeginDisabled();

        glm::vec3 lightDirection = settings.LightDirection;
        if (ImGui::DragFloat3("Fallback Light Direction", &lightDirection.x, 0.01f, -1.0f, 1.0f))
            commands.SetSceneLightDirection(scene, lightDirection);

        tracked.Begin();
        tracked.End();

        float lightIntensity = settings.LightIntensity;
        if (ImGui::DragFloat("Fallback Light Intensity", &lightIntensity, 0.01f, 0.0f, 10.0f))
            commands.SetSceneLightIntensity(scene, lightIntensity);

        tracked.Begin();
        tracked.End();

        if (hasActiveDirectionalLight)
            ImGui::EndDisabled();

        glm::vec3 ambientColor = settings.AmbientColor;
        if (ImGui::ColorEdit3("Ambient Color", &ambientColor.x))
            commands.SetSceneAmbientColor(scene, ambientColor);

        tracked.Begin();
        tracked.End();

        float ambientIntensity = settings.AmbientIntensity;
        if (ImGui::DragFloat("Ambient Intensity", &ambientIntensity, 0.01f, 0.0f, 1.0f))
            commands.SetSceneAmbientIntensity(scene, ambientIntensity);

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
                             const Project::ProjectContext& projectContext,
                             const Vulkan::TextureLibrary& textureLibrary,
                             const TrackedInspectorInteraction& tracked,
                             const float uiScale)
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

        ImGui::Spacing();

        const std::vector<std::string> textureAssetIDs =
            projectContext.EnumerateTextureAssetIDs();

        const char* texturePreview =
            actor.Material->HasBaseColorTexture()
                ? actor.Material->BaseColorTextureAssetID.c_str()
                : "None";

        if (ImGui::BeginCombo("Base Color Texture", texturePreview))
        {
            const bool noneSelected = !actor.Material->HasBaseColorTexture();
            if (ImGui::Selectable("None", noneSelected))
                commands.SetMaterialBaseColorTextureAsset(scene, actor.ID, std::string{});

            if (noneSelected) ImGui::SetItemDefaultFocus();
            if (!textureAssetIDs.empty()) ImGui::Separator();

            for (const std::string& textureAssetID : textureAssetIDs)
            {
                const bool selected = actor.Material->BaseColorTextureAssetID == textureAssetID;
                if (ImGui::Selectable(textureAssetID.c_str(), selected))
                {
                    commands.SetMaterialBaseColorTextureAsset(
                        scene,
                        actor.ID,
                        textureAssetID
                    );
                }

                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (textureAssetIDs.empty())
            ImGui::TextDisabled("No texture assets registered.");
        else if (!actor.Material->HasBaseColorTexture())
            ImGui::TextDisabled("No base color texture assigned.");
        else
        {
            const std::string& textureAssetID = actor.Material->BaseColorTextureAssetID;
            const auto textureStats = textureLibrary.GetStats(textureAssetID);

            if (!textureStats.has_value())
                ImGui::TextDisabled("Assigned texture is not loaded. Press Reload in the Assets panel.");
            else if (!textureStats->GpuReady)
                ImGui::TextDisabled("Assigned texture metadata is loaded, but the GPU texture is not ready.");
            else
            {
                ImGui::TextDisabled("Texture is sampled by the static mesh material.");

                DrawTexturePreview(
                    textureAssetID,
                    textureLibrary,
                    textureStats.value(),
                    uiScale,
                    128.0f
                );
            }
        }

        if (ImGui::Button("Remove Material"))
            commands.RemoveMaterial(scene, actor.ID);
    }

    void DrawDirectionalLightSection(Scene::Scene &scene,
                                     Scene::Actor &actor,
                                     Editor::EditorCommands &commands,
                                     const TrackedInspectorInteraction &tracked)
    {
        if (!actor.DirectionalLight.has_value())
        {
            if (ImGui::Button("Add Directional Light"))
                commands.AddDirectionalLight(scene, actor.ID);
            return;
        }

        if (!ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        bool enabled = actor.DirectionalLight->Enabled;
        if (ImGui::Checkbox("Enabled", &enabled))
            commands.SetDirectionalLightEnabled(scene, actor.ID, enabled);

        tracked.Begin();
        tracked.End();

        glm::vec3 direction = actor.DirectionalLight->Direction;
        if (ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.0f, 1.0f))
            commands.SetDirectionalLightDirection(scene, actor.ID, direction);

        tracked.Begin();
        tracked.End();

        glm::vec3 color = actor.DirectionalLight->Color;
        if (ImGui::ColorEdit3("Color", &color.x))
            commands.SetDirectionalLightColor(scene, actor.ID, color);

        tracked.Begin();
        tracked.End();

        float intensity = actor.DirectionalLight->Intensity;
        if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f))
            commands.SetDirectionalLightIntensity(scene, actor.ID, intensity);

        tracked.Begin();
        tracked.End();

        if (ImGui::Button("Remove Directional Light"))
            commands.RemoveDirectionalLight(scene, actor.ID);
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
