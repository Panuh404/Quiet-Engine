#include "Renderer/UI/Inspector/ImGuiInspectorSections.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include "ImGuiInspectorState.h"
#include "ImGuiInspectorWidgets.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/Assets/ImGuiAssetPicker.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"


namespace Quiet::Renderer::UI::Inspector
{
    void TrackedInspectorInteraction::Begin() const
    {
        if (Enabled && ImGui::IsItemActivated()) Commands.BeginInteraction(SceneRef);
    }

    void TrackedInspectorInteraction::End() const
    {
        if (Enabled && ImGui::IsItemDeactivated()) Commands.EndInteraction(SceneRef);
    }

    void DrawActorDetailsSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, EditorSizing::Px(9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorSizing::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##ActorHeaderCard", ImVec2(0.0f, EditorSizing::Px(100.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::TextUnformatted("Actor");
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
            ImGui::TextUnformatted("Scene Object");
            ImGui::PopStyleColor();

            char nameBuffer[256]{};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.GetName().c_str());
            ImGui::SetNextItemWidth(-1.0f);

            const bool nameChanged = ImGui::InputText("##ActorName", nameBuffer, sizeof(nameBuffer));
            tracked.Begin();
            if (nameChanged)
            {
                context.Commands.RenameActor(context.Scene, actor.GetID(), nameBuffer);
            }
            tracked.End();

            const std::string actorIDText = Quiet::Core::UUIDToString(actor.GetID());

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::Text("ID: %s", actorIDText.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();

        const bool canPasteTransform = state.Clipboard.Type == ComponentClipboardType::Transform;
        const auto transformHeader = BeginInspectorComponent(
            state,
            "Transform",
            "TransformComponentOptions",
            false,
            true,
            true,
            canPasteTransform,
            canPasteTransform ? nullptr : "Copy Transform values first.");

        Scene::TransformComponent& transformComponent = actor.GetComponent<Scene::TransformComponent>();
        if (transformHeader.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::Transform;
            state.Clipboard.Transform = transformComponent;
        }

        if (transformHeader.PasteRequested && canPasteTransform)
        {
            context.Commands.PasteActorTransformValues(context.Scene, actor.GetID(), state.Clipboard.Transform);
        }

        if (transformHeader.ResetRequested)
        {
            context.Commands.ResetActorTransform(context.Scene, actor.GetID());
        }

        if (transformHeader.Open)
        {
            auto& transform = transformComponent.WorldTransform;

            glm::vec3 position = transform.Position;
            if (DrawVec3PropertyRow("Position", position, 0.1f, 0.0f, 0.0f, tracked))
            {
                context.Commands.SetActorPosition(context.Scene, actor.GetID(), position);
            }

            glm::vec3 rotation = transform.RotationDegrees;
            if (DrawVec3PropertyRow("Rotation", rotation, 0.5f, 0.0f, 0.0f, tracked))
            {
                context.Commands.SetActorRotation(context.Scene, actor.GetID(), rotation);
            }

            glm::vec3 scale = transform.Scale;
            if (DrawVec3PropertyRow("Scale", scale, 0.05f, 0.01f, 100.0f, tracked))
            {
                context.Commands.SetActorScale(context.Scene, actor.GetID(), scale);
            }
        }
    }

    void DrawAddComponentMenu(InspectorContext& context, Scene::Actor& actor)
    {
        const auto meshAssetIDs = context.ProjectContext.EnumerateMeshAssetIDs();
        const bool hasStaticMesh = actor.HasComponent<Scene::StaticMeshComponent>();

        const bool canAddStaticMesh = !hasStaticMesh && !meshAssetIDs.empty();
        const bool canAddMaterial = hasStaticMesh && !actor.HasComponent<Scene::MaterialComponent>();
        const bool canAddDirectionalLight = !actor.HasComponent<Scene::DirectionalLightComponent>();
        const bool canAddRotator = !actor.HasComponent<Scene::RotatorComponent>();
        const bool canAddCamera = !actor.HasComponent<Scene::CameraComponent>();
        const bool canAddAudioSource = !actor.HasComponent<Scene::AudioSourceComponent>();
        const bool canAddAudioListener = !actor.HasComponent<Scene::AudioListenerComponent>();
        const bool canAddScript = !actor.HasComponent<Scene::ScriptComponent>();
        const bool canAddRigidBody = !actor.HasComponent<Scene::RigidBodyComponent>();

        const bool hasBoxCollider = actor.HasComponent<Scene::BoxColliderComponent>();
        const bool hasSphereCollider = actor.HasComponent<Scene::SphereColliderComponent>();
        const bool hasCapsuleCollider = actor.HasComponent<Scene::CapsuleColliderComponent>();
        const bool hasCollider = hasBoxCollider || hasSphereCollider || hasCapsuleCollider;

        const bool canAddBoxCollider = !hasCollider;
        const bool canAddSphereCollider = !hasCollider;
        const bool canAddCapsuleCollider = !hasCollider;

        const bool hasAnyAvailableComponent =
            canAddStaticMesh ||
            canAddMaterial ||
            canAddDirectionalLight ||
            canAddRotator ||
            canAddCamera ||
            canAddAudioSource ||
            canAddAudioListener ||
            canAddScript ||
            canAddRigidBody ||
            canAddBoxCollider ||
            canAddSphereCollider ||
            canAddCapsuleCollider;

        if (EditorButton("+ Add Component", ImVec2(-1.0f, 0.0f), hasAnyAvailableComponent))
        {
            ImGui::OpenPopup("##AddComponentPopup");
        }

        if (!hasAnyAvailableComponent)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped("All available components are already attached to this actor.");
            ImGui::PopStyleColor();
        }

        if (!hasStaticMesh && meshAssetIDs.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextWrapped("Static Mesh cannot be added because no mesh assets are registered.");
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopup("##AddComponentPopup"))
        {
            if (EditorMenuItem("Static Mesh", nullptr, false, canAddStaticMesh))
            {
                context.Commands.AddStaticMesh(context.Scene, actor.GetID(), context.ProjectContext, context.MeshLibrary);
                ImGui::CloseCurrentPopup();
            }

            if (!hasStaticMesh && meshAssetIDs.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Register or load a mesh asset before adding a Static Mesh component.");
            }

            if (EditorMenuItem("Material", nullptr, false, canAddMaterial))
            {
                context.Commands.AddMaterial(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (!hasStaticMesh && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("A Material component requires a Static Mesh component.");
            }

            ImGui::Separator();

            if (EditorMenuItem("Directional Light", nullptr, false, canAddDirectionalLight))
            {
                context.Commands.AddDirectionalLight(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Rotator", nullptr, false, canAddRotator))
            {
                context.Commands.AddRotator(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Camera", nullptr, false, canAddCamera))
            {
                context.Commands.AddCamera(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Audio Source", nullptr, false, canAddAudioSource))
            {
                context.Commands.AddAudioSource(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Audio Listener", nullptr, false, canAddAudioListener))
            {
                context.Commands.AddAudioListener(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Script", nullptr, false, canAddScript))
            {
                context.Commands.AddScript(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();

            if (EditorMenuItem("Rigid Body", nullptr, false, canAddRigidBody))
            {
                context.Commands.AddRigidBody(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Box Collider", nullptr, false, canAddBoxCollider))
            {
                context.Commands.AddBoxCollider(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }
            if (hasCollider && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("An Actor can currently have only one collider.");
            }

            if (EditorMenuItem("Sphere Collider", nullptr, false, canAddSphereCollider))
            {
                context.Commands.AddSphereCollider(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }
            if (hasCollider && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("An Actor can currently have only one collider.");
            }

            if (EditorMenuItem("Capsule Collider", nullptr, false, canAddCapsuleCollider))
            {
                context.Commands.AddCapsuleCollider(context.Scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }
            if (hasCollider && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("An Actor can currently have only one collider.");
            }

            ImGui::EndPopup();
        }
    }

    void DrawStaticMeshSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor)
    {
        Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        if (!staticMesh) return;

        const bool hasMeshAssets = !context.ProjectContext.EnumerateMeshAssetIDs().empty();
        const bool hasStaticMeshClipboard = state.Clipboard.Type == ComponentClipboardType::StaticMesh;

        const Project::AssetRecord* copiedMeshRecord = nullptr;
        if (hasStaticMeshClipboard && !state.Clipboard.StaticMeshResourceID.empty())
        {
            copiedMeshRecord = Project::AssetRegistryIO::FindByID(context.ProjectContext.GetAssetRegistry(), state.Clipboard.StaticMeshResourceID);
        }

        const bool copiedMeshIsRegistered = copiedMeshRecord != nullptr && copiedMeshRecord->Type == Project::AssetType::Mesh;
        const bool copiedMeshIsLoaded = copiedMeshIsRegistered && context.MeshLibrary.TryGet(state.Clipboard.StaticMeshResourceID) != nullptr;
        const bool canPasteStaticMesh = hasStaticMeshClipboard && copiedMeshIsLoaded;

        const char* pasteUnavailableReason = nullptr;
        if (!hasStaticMeshClipboard)
        {
            pasteUnavailableReason = "Copy Static Mesh values first.";
        }
        else if (!copiedMeshIsRegistered)
        {
            pasteUnavailableReason = "The copied mesh asset is no longer registered.";
        }
        else if (!copiedMeshIsLoaded)
        {
            pasteUnavailableReason = "The copied mesh asset is not currently loaded.";
        }

        const auto header = BeginInspectorComponent(
            state,
            "Static Mesh",
            "StaticMeshComponentOptions",
            true,
            false,
            true,
            canPasteStaticMesh,
            pasteUnavailableReason);

        if (header.RemoveRequested)
        {
            context.Commands.RemoveStaticMesh(context.Scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::StaticMesh;
            state.Clipboard.StaticMeshResourceID = staticMesh->MeshResourceID;
            state.Clipboard.StaticMeshVisible = staticMesh->Visible;
        }

        if (header.PasteRequested && canPasteStaticMesh)
        {
            context.Commands.PasteStaticMeshValues(
                context.Scene,
                actor.GetID(),
                state.Clipboard.StaticMeshResourceID,
                state.Clipboard.StaticMeshVisible,
                context.ProjectContext,
                context.MeshLibrary
            );
        }

        if (!header.Open) return;

        Assets::AssetPickerOptions meshPickerOptions{};
        meshPickerOptions.Type = Project::AssetType::Mesh;
        meshPickerOptions.MeshLibrary = &context.MeshLibrary;

        std::string selectedMeshAssetID;

        if (DrawAssetPickerPropertyRow("Mesh Resource", context.ProjectContext, meshPickerOptions, staticMesh->MeshResourceID, selectedMeshAssetID))
        {
            context.Commands.SetStaticMeshResource(context.Scene, actor.GetID(), selectedMeshAssetID, context.ProjectContext, context.MeshLibrary);
        }

        if (!hasMeshAssets)
        {
            DrawInspectorHint("No mesh assets registered.");
        }

        bool visible = staticMesh->Visible;
        if (DrawCheckboxPropertyRow("Visible", visible))
        {
            context.Commands.SetStaticMeshVisible(context.Scene, actor.GetID(), visible);
        }
    }

    void DrawMaterialSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor,const TrackedInspectorInteraction& tracked)
    {
        Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        Scene::MaterialComponent* material = actor.TryGetComponent<Scene::MaterialComponent>();
        if (!staticMesh || !material) return;

        const bool canResetMaterial = staticMesh->Mesh != nullptr;
        const bool hasMaterialClipboard = state.Clipboard.Type == ComponentClipboardType::Material;
        const bool materialSlotCountsMatch = hasMaterialClipboard && state.Clipboard.MaterialSlots.size() == staticMesh->MaterialSlots.size();
        const auto& assetRegistry = context.ProjectContext.GetAssetRegistry();
        const auto isRegisteredTextureAsset = [&](const std::string& textureAssetID)
        {
            if (textureAssetID.empty()) return true;
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(assetRegistry, textureAssetID);
            return record && record->Type == Project::AssetType::Texture;
        };

        bool copiedMaterialTexturesValid = true;
        if (hasMaterialClipboard)
        {
            copiedMaterialTexturesValid = isRegisteredTextureAsset(state.Clipboard.Material.BaseColorTextureAssetID);
            if (copiedMaterialTexturesValid)
            {
                for (const Scene::MaterialComponent& materialSlot : state.Clipboard.MaterialSlots)
                {
                    if (!isRegisteredTextureAsset(materialSlot.BaseColorTextureAssetID))
                    {
                        copiedMaterialTexturesValid = false;
                        break;
                    }
                }
            }
        }

        const bool canPasteMaterial = hasMaterialClipboard && materialSlotCountsMatch && copiedMaterialTexturesValid;
        const char* pasteUnavailableReason = nullptr;

        if (!hasMaterialClipboard)
        {
            pasteUnavailableReason = "Copy Material values first.";
        }
        else if (!materialSlotCountsMatch)
        {
            pasteUnavailableReason = "Source and target Materials must have the same imported slot count.";
        }
        else if (!copiedMaterialTexturesValid)
        {
            pasteUnavailableReason = "One or more copied Material textures are no longer registered in this project.";
        }

        const auto header = BeginInspectorComponent(
            state,
            "Material",
            "MaterialComponentOptions",
            true,
            canResetMaterial,
            true,
            canPasteMaterial,
            pasteUnavailableReason);

        if (header.RemoveRequested)
        {
            context.Commands.RemoveMaterial(context.Scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::Material;
            state.Clipboard.Material = *material;
            state.Clipboard.MaterialSlots = staticMesh->MaterialSlots;
        }

        if (header.PasteRequested && canPasteMaterial)
        {
            context.Commands.PasteMaterialValues(
                context.Scene,
                actor.GetID(),
                state.Clipboard.Material,
                state.Clipboard.MaterialSlots,
                context.ProjectContext);
        }

        if (header.ResetRequested)
        {
            context.Commands.ResetMaterial(context.Scene, actor.GetID());
        }

        if (!header.Open) return;

        const bool hasMaterialSlots = !staticMesh->MaterialSlots.empty();
        if (hasMaterialSlots)
        {
            DrawInspectorSubsectionHeader("Fallback Material");
            DrawInspectorHint("Used when a submesh has no matching imported material slot.");
            ImGui::Spacing();
        }

        glm::vec4 baseColor = material->BaseColor;

        if (DrawColor4PropertyRow("Base Color", baseColor, tracked))
        {
            context.Commands.SetMaterialBaseColor(context.Scene, actor.GetID(), baseColor);
        }

        ImGui::Spacing();

        const bool hasTextureAssets = !context.ProjectContext.EnumerateTextureAssetIDs().empty();

        Assets::AssetPickerOptions texturePickerOptions{};
        texturePickerOptions.Type = Project::AssetType::Texture;
        texturePickerOptions.AllowNone = true;
        texturePickerOptions.TextureLibrary = &context.TextureLibrary;

        std::string selectedTextureAssetID;

        if (DrawAssetPickerPropertyRow("Base Color Texture", context.ProjectContext, texturePickerOptions, material->BaseColorTextureAssetID, selectedTextureAssetID))
        {
            context.Commands.SetMaterialBaseColorTextureAsset(context.Scene, actor.GetID(), selectedTextureAssetID);
        }

        if (!hasTextureAssets)
        {
            DrawInspectorHint("No texture assets registered.");
        }
        else if (!material->HasBaseColorTexture())
        {
            DrawInspectorHint("No base color texture assigned.");
        }
        else
        {
            const std::string& textureAssetID = material->BaseColorTextureAssetID;
            const auto textureStats = context.TextureLibrary.GetStats(textureAssetID);

            if (!textureStats.has_value())
            {
                DrawInspectorWarning("Assigned texture is not loaded. Press Reload in the Assets panel.");
            }
            else if (!textureStats->GpuReady)
            {
                DrawInspectorWarning("Assigned texture metadata is loaded, but the GPU texture is not ready.");
            }
            else
            {
                DrawInspectorHint("Texture is sampled by the static mesh material.");
                DrawTexturePreview(textureAssetID, context.TextureLibrary, textureStats.value(), context.UIScale, 128.0f);
            }
        }

        if (hasMaterialSlots)
        {
            const std::size_t slotCount = staticMesh->MaterialSlots.size();
            const std::string subsectionLabel = "Imported Slots (" + std::to_string(slotCount) + ")";

            DrawInspectorSubsectionHeader(subsectionLabel);
            DrawInspectorHint("Imported submeshes resolve these overrides by source material index.");

            ImGui::Spacing();

            const bool openSingleSlotByDefault = slotCount == 1;
            for (std::size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
            {
                DrawMaterialSlotEditor(
                    context.Scene,
                    actor,
                    context.Commands,
                    context.ProjectContext,
                    staticMesh->MaterialSlots[slotIndex],
                    static_cast<std::uint32_t>(slotIndex),
                    context.TextureLibrary,
                    tracked,
                    hasTextureAssets,
                    openSingleSlotByDefault);
            }
        }
    }

    void DrawDirectionalLightSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        Scene::DirectionalLightComponent* light = actor.TryGetComponent<Scene::DirectionalLightComponent>();
        if (!light) return;

        const bool canPasteDirectionalLight = state.Clipboard.Type == ComponentClipboardType::DirectionalLight;

        const auto header = BeginInspectorComponent(
            state,
            "Directional Light",
            "DirectionalLightComponentOptions",
            true,
            true,
            true,
            canPasteDirectionalLight,
            canPasteDirectionalLight ? nullptr : "Copy Directional Light values first."            );

        if (header.RemoveRequested)
        {
            context.Commands.RemoveDirectionalLight(context.Scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::DirectionalLight;
            state.Clipboard.DirectionalLight = *light;
        }

        if (header.PasteRequested && canPasteDirectionalLight)
        {
            context.Commands.PasteDirectionalLightValues(context.Scene, actor.GetID(), state.Clipboard.DirectionalLight);
        }

        if (header.ResetRequested)
        {
            context.Commands.ResetDirectionalLight(context.Scene, actor.GetID());
        }

        if (!header.Open) return;

        bool enabled = light->Enabled;

        if (DrawCheckboxPropertyRow("Enabled", enabled))
        {
            context.Commands.SetDirectionalLightEnabled(context.Scene, actor.GetID(), enabled);
        }

        glm::vec3 direction = light->Direction;
        if (DrawVec3PropertyRow("Direction", direction, 0.01f, -1.0f, 1.0f, tracked))
        {
            context.Commands.SetDirectionalLightDirection(context.Scene, actor.GetID(), direction);
        }

        glm::vec3 color = light->Color;
        if (DrawColor3PropertyRow("Color", color, tracked))
        {
            context.Commands.SetDirectionalLightColor(context.Scene, actor.GetID(), color);
        }

        float intensity = light->Intensity;
        if (DrawFloatPropertyRow("Intensity", intensity, 0.01f, 0.0f, 10.0f, tracked))
        {
            context.Commands.SetDirectionalLightIntensity(context.Scene, actor.GetID(), intensity);
        }
    }

    void DrawRotatorSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        Scene::RotatorComponent* rotator = actor.TryGetComponent<Scene::RotatorComponent>();
        if (!rotator) return;

        const bool canPasteRotator = state.Clipboard.Type == ComponentClipboardType::Rotator;
        const auto header = BeginInspectorComponent(
            state,
            "Rotator",
            "RotatorComponentOptions",
            true,
            true,
            true,
            canPasteRotator,
            canPasteRotator ? nullptr : "Copy Rotator values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveRotator(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::Rotator;
            state.Clipboard.Rotator = *rotator;
        }
        if (header.PasteRequested && canPasteRotator)
        {
            context.Commands.PasteRotatorValues(context.Scene, actor.GetID(), state.Clipboard.Rotator);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetRotator(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        float rotationSpeed = rotator->RotationSpeedDegreesPerSecond;

        if (DrawFloatPropertyRow("Rotation Speed", rotationSpeed, 1.0f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetRotatorSpeed(context.Scene, actor.GetID(), rotationSpeed);
        }
    }

    void DrawCameraSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked, bool isEditMode)
    {
        Scene::CameraComponent* camera = actor.TryGetComponent<Scene::CameraComponent>();
        if (!camera) return;

        const bool canPasteCamera = state.Clipboard.Type == ComponentClipboardType::Camera;
        const auto header = BeginInspectorComponent(
            state,
            "Camera",
            "CameraComponentOptions",
            true,
            true,
            true,
            canPasteCamera,
            canPasteCamera ? nullptr : "Copy Camera values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveCamera(context.Scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::Camera;
            state.Clipboard.Camera = *camera;
        }
        if (header.PasteRequested && canPasteCamera)
        {
            context.Commands.PasteCameraValues(context.Scene, actor.GetID(), state.Clipboard.Camera);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetCamera(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        ImGui::PushID("CameraComponentContent");
        const bool isPilotingThisCamera =
            context.EditorState.Selection.PilotedCameraActorID.has_value() &&
            context.EditorState.Selection.PilotedCameraActorID.value() == actor.GetID();

        if (isEditMode)
        {
            if (!isPilotingThisCamera)
            {
                if (EditorButton("Pilot Camera"))
                {
                    context.EditorState.Selection.PilotedCameraActorID = actor.GetID();
                }
            }
            else
            {
                if (EditorButton("Stop Piloting"))
                {
                    context.EditorState.Selection.PilotedCameraActorID.reset();
                }
            }
        }

        bool primary = camera->Primary;
        if (DrawCheckboxPropertyRow("Primary", primary))
        {
            context.Commands.SetCameraPrimary(context.Scene, actor.GetID(), primary);
        }

        float fov = camera->FieldOfViewDegrees;
        if (DrawFloatPropertyRow("Field Of View", fov, 0.25f, 1.0f, 179.0f, tracked))
        {
            context.Commands.SetCameraFieldOfView(context.Scene, actor.GetID(), fov);
        }

        const float currentFarClip = std::isfinite(camera->FarClip) ? camera->FarClip : 1000.0f;
        const float maximumNearClip = std::clamp(currentFarClip - 0.001f, 0.001f, 100.0f);
        float nearClip = camera->NearClip;

        if (DrawFloatPropertyRow("Near Clip", nearClip, 0.01f, 0.001f, maximumNearClip, tracked))
        {
            context.Commands.SetCameraNearClip(context.Scene, actor.GetID(), nearClip);
        }

        const float currentNearClip = std::isfinite(camera->NearClip) ? std::clamp(camera->NearClip, 0.001f, 100.0f) : 0.1f;
        const float minimumFarClip = std::max(1.0f, currentNearClip + 0.001f);
        float farClip = camera->FarClip;

        if (DrawFloatPropertyRow("Far Clip", farClip, 1.0f, minimumFarClip, 100000.0f, tracked))
        {
            context.Commands.SetCameraFarClip(context.Scene, actor.GetID(), farClip);
        }
        ImGui::PopID();
    }

    void DrawAudioSourceSection(InspectorContext &context, InspectorState &state, Scene::Actor &actor, const TrackedInspectorInteraction &tracked)
    {
        Scene::AudioSourceComponent* audioSource = actor.TryGetComponent<Scene::AudioSourceComponent>();
        if (!audioSource) return;

        const bool canPasteAudioSource = state.Clipboard.Type == ComponentClipboardType::AudioSource;
        const auto header = BeginInspectorComponent(
            state,
            "Audio Source",
            "AudioSourceComponentOptions",
            true,
            true,
            true,
            canPasteAudioSource,
            canPasteAudioSource ? nullptr : "Copy Audio Source values first.");
        if (header.RemoveRequested)
        {
            context.Commands.RemoveAudioSource(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::AudioSource;
            state.Clipboard.AudioSource = *audioSource;
        }
        if (header.PasteRequested && canPasteAudioSource)
        {
            context.Commands.PasteAudioSourceValues(context.Scene, actor.GetID(), state.Clipboard.AudioSource, context.ProjectContext);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetAudioSource(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        Assets::AssetPickerOptions audioPickerOptions{};
        audioPickerOptions.Type = Project::AssetType::Audio;
        audioPickerOptions.AllowNone = true;

        std::string selectedAudioAssetID;
        if (DrawAssetPickerPropertyRow("Audio", context.ProjectContext, audioPickerOptions, audioSource->AudioAssetID, selectedAudioAssetID))
        {
            context.Commands.SetAudioSourceAsset(context.Scene, actor.GetID(), selectedAudioAssetID, context.ProjectContext);
        }

        const auto getAudioBusLabel = [](const Audio::AudioBus bus)
        {
            switch (bus)
            {
                case Audio::AudioBus::Master:   return "Master";
                case Audio::AudioBus::Music:    return "Music";
                case Audio::AudioBus::SFX:      return "SFX";
                case Audio::AudioBus::UI:       return "UI";
                default:                        return "SFX";
            }
        };

        Audio::AudioBus bus = audioSource->Bus;
        bool busChanged = false;

        ImGui::PushID("AudioSourceBus");
        BeginInspectorPropertyRow("Bus");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (EditorBeginCombo("##Value", getAudioBusLabel(bus)))
        {
            const auto drawBusOption = [&](const char* label, const Audio::AudioBus option)
            {
                const bool selected = bus == option;
                if (EditorSelectable(label, selected))
                {
                    bus = option;
                    busChanged = true;
                }
                if (selected)ImGui::SetItemDefaultFocus();
            };

            drawBusOption("Master", Audio::AudioBus::Master);
            drawBusOption("Music", Audio::AudioBus::Music);
            drawBusOption("SFX", Audio::AudioBus::SFX);
            drawBusOption("UI", Audio::AudioBus::UI);

            ImGui::EndCombo();
        }

        ImGui::PopID();

        if (busChanged)
        {
            context.Commands.SetAudioSourceBus(context.Scene, actor.GetID(), bus);
        }

        float volume = audioSource->Volume;
        if (DrawFloatPropertyRow("Volume", volume, 0.01f, 0.0f, 4.0f, tracked))
        {
            context.Commands.SetAudioSourceVolume(context.Scene, actor.GetID(), volume);
        }

        float pitch = audioSource->Pitch;
        if (DrawFloatPropertyRow("Pitch", pitch, 0.01f, 0.01f, 4.0f, tracked))
        {
            context.Commands.SetAudioSourcePitch(context.Scene, actor.GetID(), pitch);
        }

        bool playOnStart = audioSource->PlayOnStart;
        if (DrawCheckboxPropertyRow("Play On Start", playOnStart))
        {
            context.Commands.SetAudioSourcePlayOnStart(context.Scene, actor.GetID(), playOnStart);
        }

        bool loop = audioSource->Loop;
        if (DrawCheckboxPropertyRow("Loop", loop))
        {
            context.Commands.SetAudioSourceLoop(context.Scene, actor.GetID(), loop);
        }

        bool spatial = audioSource->Spatial;
        if (DrawCheckboxPropertyRow("Spatial", spatial))
        {
            context.Commands.SetAudioSourceSpatial(context.Scene, actor.GetID(), spatial);
        }

        if (audioSource->Spatial)
        {
            const float currentMaxDistance = std::isfinite(audioSource->MaxDistance) ? std::clamp(audioSource->MaxDistance, 0.02f, 100000.0f) : 100000.0f;
            const float maximumMinDistance = std::clamp(currentMaxDistance - 0.01f, 0.01f, 99999.99f);
            float minDistance = audioSource->MinDistance;
            if (DrawFloatPropertyRow("Min Distance", minDistance, 0.1f, 0.01f, maximumMinDistance, tracked))
            {
                context.Commands.SetAudioSourceMinDistance(context.Scene, actor.GetID(), minDistance);
            }

            const float currentMinDistance = std::isfinite(audioSource->MinDistance) ? std::clamp(audioSource->MinDistance, 0.01f, 99999.99f) : 1.0f;
            const float minimumMaxDistance = std::min(100000.0f, currentMinDistance + 0.01f);
            float maxDistance = audioSource->MaxDistance;
            if (DrawFloatPropertyRow("Max Distance", maxDistance, 1.0f, minimumMaxDistance, 100000.0f, tracked))
            {
                context.Commands.SetAudioSourceMaxDistance(context.Scene, actor.GetID(), maxDistance);
            }

            float rolloff = audioSource->Rolloff;
            if (DrawFloatPropertyRow("Rolloff", rolloff, 0.05f, 0.0f, 10.0f, tracked))
            {
                context.Commands.SetAudioSourceRolloff(context.Scene, actor.GetID(), rolloff);
            }

            const float currentOuterAngle = std::isfinite(audioSource->ConeOuterAngleDegrees) ? std::clamp(audioSource->ConeOuterAngleDegrees, 0.0f, 360.0f) : 360.0f;
            float coneInnerAngle = audioSource->ConeInnerAngleDegrees;
            if (DrawFloatPropertyRow("Cone Inner Angle", coneInnerAngle, 1.0f, 0.0f, currentOuterAngle, tracked))
            {
                context.Commands.SetAudioSourceConeInnerAngle(context.Scene, actor.GetID(), coneInnerAngle);
            }

            const float currentInnerAngle = std::isfinite(audioSource->ConeInnerAngleDegrees) ? std::clamp(audioSource->ConeInnerAngleDegrees, 0.0f, 360.0f) : 360.0f;
            float coneOuterAngle = audioSource->ConeOuterAngleDegrees;
            if (DrawFloatPropertyRow("Cone Outer Angle", coneOuterAngle, 1.0f, currentInnerAngle, 360.0f, tracked))
            {
                context.Commands.SetAudioSourceConeOuterAngle(context.Scene, actor.GetID(), coneOuterAngle);
            }

            float coneOuterGain = audioSource->ConeOuterGain;
            if (DrawFloatPropertyRow("Cone Outer Gain", coneOuterGain, 0.01f, 0.0f, 1.0f, tracked))
            {
                context.Commands.SetAudioSourceConeOuterGain(context.Scene, actor.GetID(), coneOuterGain);
            }

            float dopplerFactor = audioSource->DopplerFactor;
            if (DrawFloatPropertyRow("Doppler Factor", dopplerFactor, 0.05f, 0.0f, 10.0f, tracked))
            {
                context.Commands.SetAudioSourceDopplerFactor(context.Scene, actor.GetID(), dopplerFactor);
            }
        }

        if (audioSource->AudioAssetID.empty())
        {
            ImGui::Spacing();

            if (context.ProjectContext.EnumerateAudioAssetIDs().empty())
            {
                DrawInspectorHint("No audio assets are registered.");
            }
            else
            {
                DrawInspectorHint("Assign an Audio asset to this source.");
            }
        }
    }

    void DrawAudioListenerSection(InspectorContext &context, InspectorState &state, Scene::Actor &actor)
    {
        Scene::AudioListenerComponent* listener = actor.TryGetComponent<Scene::AudioListenerComponent>();
        if (!listener) return;

        const bool canPasteAudioListener = state.Clipboard.Type == ComponentClipboardType::AudioListener;

        const auto header = BeginInspectorComponent(
            state,
            "Audio Listener",
            "AudioListenerComponentOptions",
            true,
            true,
            true,
            canPasteAudioListener,
            canPasteAudioListener ? nullptr : "Copy Audio Listener values first.");
        if (header.RemoveRequested)
        {
            context.Commands.RemoveAudioListener(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::AudioListener;
            state.Clipboard.AudioListener = *listener;
        }
        if (header.PasteRequested && canPasteAudioListener)
        {
            context.Commands.PasteAudioListenerValues(context.Scene, actor.GetID(), state.Clipboard.AudioListener);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetAudioListener(context.Scene, actor.GetID());
        }
        if (!header.Open) return;
        ImGui::PushID("AudioListenerComponentContent");

        bool primary = listener->Primary;

        if (DrawCheckboxPropertyRow("Primary", primary))
        {
            context.Commands.SetAudioListenerPrimary(context.Scene, actor.GetID(), primary);
        }

        if (listener->Primary)
        {
            ImGui::Spacing();
            DrawInspectorHint("This listener will be used for runtime spatial audio.");
        }
        ImGui::PopID();
    }

    void DrawScriptSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        Scene::ScriptComponent* script = actor.TryGetComponent<Scene::ScriptComponent>();
        if (!script) return;

        const bool canPasteScript = state.Clipboard.Type == ComponentClipboardType::Script;
        const auto header = BeginInspectorComponent(
            state,
            "Script",
            "ScriptComponentOptions",
            true,
            true,
            true,
            canPasteScript,
            canPasteScript ? nullptr : "Copy Script values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveScript(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::Script;
            state.Clipboard.Script = *script;
        }
        if (header.PasteRequested && canPasteScript)
        {
            context.Commands.PasteScriptValues(context.Scene, actor.GetID(), state.Clipboard.Script, context.ProjectContext);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetScript(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        Assets::AssetPickerOptions pickerOptions{};
        pickerOptions.Type = Project::AssetType::Script;
        pickerOptions.AllowNone = true;

        std::string selectedScriptAssetID;

        if (DrawAssetPickerPropertyRow("Script", context.ProjectContext, pickerOptions, script->ScriptAssetID, selectedScriptAssetID))
        {
            context.Commands.SetScriptAsset(context.Scene, actor.GetID(), selectedScriptAssetID, context.ProjectContext);
        }

        if (!script->ScriptAssetID.empty())
        {
            ImGui::Spacing();

            const auto sourceError = context.EditorState.Scripting.SourceErrors.find(script->ScriptAssetID);
            if (sourceError != context.EditorState.Scripting.SourceErrors.end())
            {
                ImGui::Spacing();

                DrawInspectorWarning("Script source error:");
                DrawInspectorWarning(sourceError->second.c_str());
            }

            if (EditorButton("Refresh Exposed Properties", ImVec2(-1.0f, 0.0f)))
            {
                context.Commands.RefreshScriptProperties(context.Scene, actor.GetID(), context.ProjectContext);
                return;
            }

            if (!script->Properties.empty())
            {
                DrawInspectorSubsectionHeader("Exposed Properties");

                for (const Scene::ScriptProperty& property : script->Properties)
                {
                    switch (property.Type)
                    {
                        case Scene::ScriptPropertyType::Float:
                        {
                            Scene::ScriptProperty edited = property;
                            float value = property.FloatValue;
                            if (DrawFloatPropertyRow(property.Name.c_str(), value, 0.1f, 0.0f, 0.0f, tracked))
                            {
                                edited.FloatValue = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Int:
                        {
                            Scene::ScriptProperty edited = property;
                            std::int64_t value = property.IntValue;
                            if (DrawIntPropertyRow(property.Name.c_str(), value, tracked))
                            {
                                edited.IntValue = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Bool:
                        {
                            Scene::ScriptProperty edited = property;
                            bool value = property.BoolValue;
                            if (DrawCheckboxPropertyRow(property.Name.c_str(), value))
                            {
                                edited.BoolValue = value;
                                context.Commands.SetScriptPropertyValueImmediate(context.Scene, actor.GetID(), edited, context.ProjectContext);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::String:
                        {
                            Scene::ScriptProperty edited = property;
                            std::string value = property.StringValue;
                            if (DrawStringPropertyRow(property.Name.c_str(), value, tracked))
                            {
                                edited.StringValue = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Vector2:
                        {
                            Scene::ScriptProperty edited = property;
                            glm::vec2 value = property.Vector2Value;
                            if (DrawVec2PropertyRow(property.Name.c_str(), value, 0.1f, 0.0f, 0.0f, tracked))
                            {
                                edited.Vector2Value = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Vector3:
                        {
                            Scene::ScriptProperty edited = property;
                            glm::vec3 value = property.Vector3Value;
                            if (DrawVec3PropertyRow(property.Name.c_str(), value, 0.1f, 0.0f, 0.0f, tracked))
                            {
                                edited.Vector3Value = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Vector4:
                        {
                            Scene::ScriptProperty edited = property;
                            glm::vec4 value = property.Vector4Value;
                            if (DrawVec4PropertyRow(property.Name.c_str(), value, 0.1f, 0.0f, 0.0f, tracked))
                            {
                                edited.Vector4Value = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Color3:
                        {
                            Scene::ScriptProperty edited = property;
                            glm::vec3 value = property.Color3Value;
                            if (DrawColor3PropertyRow(property.Name.c_str(), value, tracked))
                            {
                                edited.Color3Value = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Color4:
                        {
                            Scene::ScriptProperty edited = property;
                            glm::vec4 value = property.Color4Value;
                            if (DrawColor4PropertyRow(property.Name.c_str(), value, tracked))
                            {
                                edited.Color4Value = value;
                                context.Commands.SetScriptPropertyValue(context.Scene, actor.GetID(), edited);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::Actor:
                        {
                            Scene::ScriptProperty edited = property;
                            Core::UUID value = property.ActorValue;
                            if (DrawActorPickerPropertyRow(property.Name.c_str(), context.Scene, property.ActorValue, value))
                            {
                                edited.ActorValue = value;
                                context.Commands.SetScriptPropertyValueImmediate(context.Scene, actor.GetID(), edited, context.ProjectContext);
                            }
                            break;
                        }

                        case Scene::ScriptPropertyType::MeshAsset:
                        case Scene::ScriptPropertyType::TextureAsset:
                        case Scene::ScriptPropertyType::AudioAsset:
                        case Scene::ScriptPropertyType::ScriptAsset:
                        {
                            Scene::ScriptProperty edited = property;
                            Assets::AssetPickerOptions options{};
                            options.AllowNone = true;

                            switch (property.Type)
                            {
                                case Scene::ScriptPropertyType::MeshAsset:
                                    options.Type = Project::AssetType::Mesh;
                                    options.MeshLibrary = &context.MeshLibrary;
                                    break;

                                case Scene::ScriptPropertyType::TextureAsset:
                                    options.Type = Project::AssetType::Texture;
                                    options.TextureLibrary = &context.TextureLibrary;
                                    break;

                                case Scene::ScriptPropertyType::AudioAsset:
                                    options.Type = Project::AssetType::Audio;
                                    break;

                                case Scene::ScriptPropertyType::ScriptAsset:
                                    options.Type = Project::AssetType::Script;
                                    break;

                                default:
                                    break;
                            }

                            std::string selectedAssetID;
                            if (DrawAssetPickerPropertyRow(property.Name.c_str(), context.ProjectContext, options, property.AssetValue, selectedAssetID))
                            {
                                edited.AssetValue = selectedAssetID;
                                context.Commands.SetScriptPropertyValueImmediate(context.Scene, actor.GetID(), edited, context.ProjectContext);
                            }
                            break;
                        }
                    }
                }
            }
            else
            {
                ImGui::Spacing();
                DrawInspectorHint("No exposed properties declared.");
            }
        }
        else
        {
            ImGui::Spacing();
            if (context.ProjectContext.EnumerateScriptAssetIDs().empty())
            {
                DrawInspectorHint("No Lua script assets are registered.");
            }
            else
            {
                DrawInspectorHint("Assign a Lua Script asset.");
            }
        }
    }

    void DrawRigidBodySection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        Scene::RigidBodyComponent* rigidBody = actor.TryGetComponent<Scene::RigidBodyComponent>();
        if (!rigidBody) return;

        const bool canPasteRigidBody = state.Clipboard.Type == ComponentClipboardType::RigidBody;

        const auto header = BeginInspectorComponent(
            state,
            "Rigid Body",
            "RigidBodyComponentOptions",
            true,
            true,
            true,
            canPasteRigidBody,
            canPasteRigidBody ? nullptr : "Copy Rigid Body values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveRigidBody(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::RigidBody;
            state.Clipboard.RigidBody = *rigidBody;
        }
        if (header.PasteRequested && canPasteRigidBody)
        {
            context.Commands.PasteRigidBodyValues(context.Scene, actor.GetID(), state.Clipboard.RigidBody);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetRigidBody(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        Scene::RigidBodyMotionType motionType = rigidBody->MotionType;
        if (DrawRigidBodyMotionTypePropertyRow("Motion Type", motionType))
        {
            context.Commands.SetRigidBodyMotionType(context.Scene, actor.GetID(), motionType);
        }

        float friction = rigidBody->Friction;
        if (DrawFloatPropertyRow("Friction", friction, 0.01f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetRigidBodyFriction(context.Scene, actor.GetID(), friction);
        }

        float restitution = rigidBody->Restitution;
        if (DrawFloatPropertyRow("Restitution", restitution, 0.01f, 0.0f, 1.0f, tracked))
        {
            context.Commands.SetRigidBodyRestitution(context.Scene, actor.GetID(), restitution);
        }

        float linearDamping = rigidBody->LinearDamping;
        if (DrawFloatPropertyRow("Linear Damping", linearDamping, 0.01f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetRigidBodyLinearDamping(context.Scene, actor.GetID(), linearDamping);
        }

        float angularDamping = rigidBody->AngularDamping;
        if (DrawFloatPropertyRow("Angular Damping", angularDamping, 0.01f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetRigidBodyAngularDamping(context.Scene, actor.GetID(), angularDamping);
        }

        float gravityFactor = rigidBody->GravityFactor;
        if (DrawFloatPropertyRow("Gravity Factor", gravityFactor, 0.05f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetRigidBodyGravityFactor(context.Scene, actor.GetID(), gravityFactor);
        }

        bool allowSleeping = rigidBody->AllowSleeping;
        if (DrawCheckboxPropertyRow("Allow Sleeping", allowSleeping))
        {
            context.Commands.SetRigidBodyAllowSleeping(context.Scene, actor.GetID(), allowSleeping);
        }

        const bool hasCollider =
            actor.HasComponent<Scene::BoxColliderComponent>() ||
            actor.HasComponent<Scene::SphereColliderComponent>() ||
            actor.HasComponent<Scene::CapsuleColliderComponent>();
        if (!hasCollider)
        {
            ImGui::Spacing();
            DrawInspectorWarning("This Rigid Body has no collider. It will not create a physics body until a supported collider is attached.");
        }
    }

    void DrawBoxColliderSection(InspectorContext& context, InspectorState& state, Scene::Actor& actor, const TrackedInspectorInteraction& tracked)
    {
        Scene::BoxColliderComponent* collider = actor.TryGetComponent<Scene::BoxColliderComponent>();
        if (!collider) return;

        const bool canPasteBoxCollider = state.Clipboard.Type == ComponentClipboardType::BoxCollider;

        const auto header = BeginInspectorComponent(
            state,
            "Box Collider",
            "BoxColliderComponentOptions",
            true,
            true,
            true,
            canPasteBoxCollider,
            canPasteBoxCollider ? nullptr : "Copy Box Collider values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveBoxCollider(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type =ComponentClipboardType::BoxCollider;
            state.Clipboard.BoxCollider = *collider;
        }
        if (header.PasteRequested && canPasteBoxCollider)
        {
            context.Commands.PasteBoxColliderValues(context.Scene, actor.GetID(), state.Clipboard.BoxCollider);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetBoxCollider(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        glm::vec3 size = collider->Size;
        if (DrawVec3PropertyRow("Size", size, 0.05f, 0.001f, 100000.0f, tracked))
        {
            context.Commands.SetBoxColliderSize(context.Scene, actor.GetID(), size);
        }

        glm::vec3 center = collider->Center;
        if (DrawVec3PropertyRow("Center", center, 0.05f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetBoxColliderCenter(context.Scene, actor.GetID(), center);
        }

        if (!actor.HasComponent<Scene::RigidBodyComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This collider has no Rigid Body. It is authored but will not participate in runtime physics yet.");
        }
        if (actor.HasComponent<Scene::SphereColliderComponent>() || actor.HasComponent<Scene::CapsuleColliderComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This Actor has multiple collider components. Runtime physics will skip it.");
        }
    }

    void DrawSphereColliderSection(InspectorContext &context, InspectorState &state, Scene::Actor &actor, const TrackedInspectorInteraction &tracked)
    {
        Scene::SphereColliderComponent* collider = actor.TryGetComponent<Scene::SphereColliderComponent>();
        if (!collider) return;

        const bool canPasteSphereCollider = state.Clipboard.Type == ComponentClipboardType::SphereCollider;
        const auto header = BeginInspectorComponent(
            state,
            "Sphere Collider",
            "SphereColliderComponentOptions",
            true,
            true,
            true,
            canPasteSphereCollider,
            canPasteSphereCollider ? nullptr : "Copy Sphere Collider values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveSphereCollider(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::SphereCollider;
            state.Clipboard.SphereCollider = *collider;
        }
        if (header.PasteRequested && canPasteSphereCollider)
        {
            context.Commands.PasteSphereColliderValues(context.Scene, actor.GetID(), state.Clipboard.SphereCollider);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetSphereCollider(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        float radius = collider->Radius;
        if (DrawFloatPropertyRow("Radius", radius, 0.01f, 0.001f, 100000.0f, tracked))
        {
            context.Commands.SetSphereColliderRadius(context.Scene, actor.GetID(), radius);
        }

        glm::vec3 center = collider->Center;
        if (DrawVec3PropertyRow("Center", center, 0.05f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetSphereColliderCenter(context.Scene, actor.GetID(), center);
        }

        if (!actor.HasComponent<Scene::RigidBodyComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This collider has no Rigid Body. It is authored but will not participate in runtime physics yet.");
        }

        if (actor.HasComponent<Scene::BoxColliderComponent>() || actor.HasComponent<Scene::CapsuleColliderComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This Actor has multiple collider components. Runtime physics will skip it.");
        }
    }

    void DrawCapsuleColliderSection(InspectorContext &context, InspectorState &state, Scene::Actor &actor, const TrackedInspectorInteraction &tracked)
    {
        Scene::CapsuleColliderComponent* collider = actor.TryGetComponent<Scene::CapsuleColliderComponent>();
        if (!collider) return;

        const bool canPasteCapsuleCollider = state.Clipboard.Type == ComponentClipboardType::CapsuleCollider;
        const auto header = BeginInspectorComponent(
            state,
            "Capsule Collider",
            "CapsuleColliderComponentOptions",
            true,
            true,
            true,
            canPasteCapsuleCollider,
            canPasteCapsuleCollider ? nullptr : "Copy Capsule Collider values first.");

        if (header.RemoveRequested)
        {
            context.Commands.RemoveCapsuleCollider(context.Scene, actor.GetID());
            return;
        }
        if (header.CopyRequested)
        {
            state.Clipboard.Type = ComponentClipboardType::CapsuleCollider;
            state.Clipboard.CapsuleCollider = *collider;
        }
        if (header.PasteRequested && canPasteCapsuleCollider)
        {
            context.Commands.PasteCapsuleColliderValues(context.Scene, actor.GetID(), state.Clipboard.CapsuleCollider);
        }
        if (header.ResetRequested)
        {
            context.Commands.ResetCapsuleCollider(context.Scene, actor.GetID());
        }
        if (!header.Open) return;

        float radius = collider->Radius;

        if (DrawFloatPropertyRow("Radius", radius, 0.01f, 0.001f, 49999.0f, tracked))
        {
            context.Commands.SetCapsuleColliderRadius(context.Scene, actor.GetID(), radius);
        }

        float height = collider->Height;
        const float minimumHeight = collider->Radius * 2.0f + 0.001f;

        if (DrawFloatPropertyRow("Height", height, 0.05f, minimumHeight, 100000.0f, tracked))
        {
            context.Commands.SetCapsuleColliderHeight(context.Scene, actor.GetID(), height);
        }

        glm::vec3 center = collider->Center;
        if (DrawVec3PropertyRow("Center", center, 0.05f, 0.0f, 0.0f, tracked))
        {
            context.Commands.SetCapsuleColliderCenter(context.Scene, actor.GetID(), center);
        }

        if (!actor.HasComponent<Scene::RigidBodyComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This collider has no Rigid Body. It is authored but will not participate in runtime physics yet.");
        }

        if (actor.HasComponent<Scene::BoxColliderComponent>() || actor.HasComponent<Scene::SphereColliderComponent>())
        {
            ImGui::Spacing();
            DrawInspectorWarning("This Actor has multiple collider components. Runtime physics will skip it.");
        }
    }
}
