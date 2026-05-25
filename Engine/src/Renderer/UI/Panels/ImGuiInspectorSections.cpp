#include "ImGuiInspectorSections.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;

    struct InspectorComponentHeaderResult
    {
        bool Open = false;
        bool RemoveRequested = false;
        bool ResetRequested = false;
    };

    std::string g_OpenInspectorComponentMenuID;
    ImVec2 g_OpenInspectorComponentMenuPosition = ImVec2(0.0f, 0.0f);
    int g_OpenInspectorComponentMenuFrame = -1;

    ImVec2 ClampMenuPositionToViewport(const ImVec2 desiredPosition,
                                       const ImVec2 menuSize)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        const float minX = viewport->WorkPos.x + 8.0f;
        const float minY = viewport->WorkPos.y + 8.0f;
        const float maxX = viewport->WorkPos.x + viewport->WorkSize.x - menuSize.x - 8.0f;
        const float maxY = viewport->WorkPos.y + viewport->WorkSize.y - menuSize.y - 8.0f;

        return ImVec2(
            std::clamp(desiredPosition.x, minX, maxX),
            std::clamp(desiredPosition.y, minY, maxY)
        );
    }

    void DrawFloatingComponentOptionsMenu(const char* popupID,
                                          const bool removable,
                                          const bool resettable,
                                          InspectorComponentHeaderResult& result)
    {
        if (g_OpenInspectorComponentMenuID != popupID)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            g_OpenInspectorComponentMenuID.clear();
            return;
        }

        constexpr float menuWidth = 230.0f;
        constexpr float estimatedMenuHeight = 48.0f;

        const ImVec2 menuSize(menuWidth, estimatedMenuHeight);
        const ImVec2 menuPosition = ClampMenuPositionToViewport(
            g_OpenInspectorComponentMenuPosition,
            menuSize
        );

        const std::string windowID = std::string("##FloatingComponentMenu_") + popupID;

        ImGui::SetNextWindowPos(menuPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(menuWidth, 0.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 6.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, QUI::EditorColor::Background1);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::Border);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.160f, 0.055f, 0.065f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.270f, 0.080f, 0.095f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.360f, 0.095f, 0.115f, 1.0f));

        bool hovered = false;

        if (ImGui::Begin(windowID.c_str(), nullptr, flags))
        {
            hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            const ImVec2 itemSize(ImGui::GetContentRegionAvail().x, 0.0f);

            if (resettable)
            {
                if (QUI::EditorSelectable(
                        "Reset Transform",
                        false,
                        0,
                        itemSize))
                {
                    result.ResetRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }

                ImGui::Separator();
            }

            if (removable)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);

                if (QUI::EditorSelectable(
                        "Remove Component",
                        false,
                        0,
                        itemSize))
                {
                    result.RemoveRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }

                ImGui::PopStyleColor();
            }
            else
            {
                QUI::EditorSelectable(
                    "Transform cannot be removed",
                    false,
                    0,
                    itemSize,
                    false
                );
            }
        }

        ImGui::End();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(5);

        const bool openedThisFrame =
            g_OpenInspectorComponentMenuFrame == ImGui::GetFrameCount();

        if (!openedThisFrame &&
            !hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            g_OpenInspectorComponentMenuID.clear();
        }
    }

    InspectorComponentHeaderResult BeginInspectorComponent(const char* title,
                                                           const char* popupID,
                                                           const bool removable = true,
                                                           const bool resettable = false)
    {
        InspectorComponentHeaderResult result{};

        ImGui::PushID(popupID);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, QUI::EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, QUI::EditorColor::AccentDim);

        result.Open = ImGui::CollapsingHeader(
            title,
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap
        );

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const float buttonSize = ImGui::GetFrameHeight();
        const float rightX = ImGui::GetWindowContentRegionMax().x - buttonSize;

        ImGui::SameLine();
        ImGui::SetCursorPosX(rightX);

        const char* gearButtonID = "##ComponentOptionsButton";
        const char* gearIcon = QUI::Icons::Gear;

        ImGui::InvisibleButton(gearButtonID, ImVec2(buttonSize, buttonSize));

        const bool gearHovered = ImGui::IsItemHovered();
        const bool gearPressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (gearHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const ImVec2 gearMin = ImGui::GetItemRectMin();
        const ImVec2 gearMax = ImGui::GetItemRectMax();

        const ImVec2 iconSize = ImGui::CalcTextSize(gearIcon);
        const ImVec2 iconPos = ImVec2(
            gearMin.x + (buttonSize - iconSize.x) * 0.5f,
            gearMin.y + (buttonSize - iconSize.y) * 0.5f
        );

        const ImVec4 iconColor =
            gearHovered
                ? QUI::EditorColor::AccentHover
                : QUI::EditorColor::TextMuted;

        ImGui::GetWindowDrawList()->AddText(
            iconPos,
            ImGui::ColorConvertFloat4ToU32(iconColor),
            gearIcon
        );

        if (gearPressed)
        {
            if (g_OpenInspectorComponentMenuID == popupID)
            {
                g_OpenInspectorComponentMenuID.clear();
            }
            else
            {
                constexpr float menuWidth = 230.0f;
                g_OpenInspectorComponentMenuID = popupID;
                g_OpenInspectorComponentMenuFrame = ImGui::GetFrameCount();
                g_OpenInspectorComponentMenuPosition = ImVec2(
                    gearMax.x - menuWidth,
                    gearMax.y + 4.0f
                );
            }
        }
        else if (g_OpenInspectorComponentMenuID == popupID)
        {
            constexpr float menuWidth = 230.0f;
            g_OpenInspectorComponentMenuPosition = ImVec2(
                gearMax.x - menuWidth,
                gearMax.y + 4.0f
            );
        }

        ImGui::PopID();
        DrawFloatingComponentOptionsMenu(popupID, removable, resettable, result);
        return result;
    }

    void DrawInspectorHint(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    void DrawInspectorWarning(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Warning);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    void DrawMaterialTexturePreviewIfAvailable(
        const std::string& textureAssetID,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const float uiScale,
        const float previewSize)
    {
        if (textureAssetID.empty())
            return;

        const auto textureStats = textureLibrary.GetStats(textureAssetID);
        if (!textureStats.has_value())
        {
            DrawInspectorWarning("Texture is not loaded. Press Reload in the Assets panel.");
            return;
        }

        if (!textureStats->GpuReady)
        {
            DrawInspectorWarning("Texture metadata is loaded, but GPU texture is not ready.");
            return;
        }

        QUI::DrawTexturePreview(
            textureAssetID,
            textureLibrary,
            textureStats.value(),
            uiScale,
            previewSize
        );
    }

    float GetInspectorPropertyLabelWidth()
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        return std::clamp(availableWidth * 0.28f, 92.0f, 128.0f);
    }

    void BeginInspectorPropertyRow(const char* label)
    {
        const float rowStartX = ImGui::GetCursorPosX();
        const float labelWidth = GetInspectorPropertyLabelWidth();

        ImGui::AlignTextToFramePadding();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::SameLine(rowStartX + labelWidth);
    }

    bool DrawFloatPropertyRow(const char* label,
                              float& value,
                              const float speed,
                              const float minValue,
                              const float maxValue,
                              const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::DragFloat(
            "##Value",
            &value,
            speed,
            minValue,
            maxValue,
            "%.3f"
        );

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool DrawVec3PropertyRow(const char* label,
                             glm::vec3& value,
                             const float speed,
                             const float minValue,
                             const float maxValue,
                             const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginInspectorPropertyRow(label);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float axisLabelWidth = 14.0f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        const float fieldWidth = std::max(
            54.0f,
            (availableWidth - axisLabelWidth * 3.0f - spacing * 5.0f) / 3.0f
        );

        bool changed = false;

        auto drawAxis = [&](const char* axisLabel, float& axisValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(axisLabel);
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::DragFloat(
                "##Axis",
                &axisValue,
                speed,
                minValue,
                maxValue,
                "%.3f"
            );

            tracked.Begin();
            tracked.End();
        };

        ImGui::PushID("X");
        drawAxis("X", value.x);
        ImGui::PopID();

        ImGui::SameLine();

        ImGui::PushID("Y");
        drawAxis("Y", value.y);
        ImGui::PopID();

        ImGui::SameLine();

        ImGui::PushID("Z");
        drawAxis("Z", value.z);
        ImGui::PopID();

        ImGui::PopID();

        return changed;
    }

    bool DrawCheckboxPropertyRow(const char* label, bool& value)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        const bool changed = QUI::EditorCheckbox("##Value", &value);

        ImGui::PopID();
        return changed;
    }

    bool BeginComboPropertyRow(const char* label, const char* previewValue)
    {
        BeginInspectorPropertyRow(label);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const std::string comboID = std::string("##") + label;
        return QUI::EditorBeginCombo(comboID.c_str(), previewValue);
    }

    bool DrawColor3PropertyRow(const char* label,
                               glm::vec3& value,
                               const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::ColorEdit3("##Value", &value.x);

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool DrawColor4PropertyRow(const char* label,
                               glm::vec4& value,
                               const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::ColorEdit4("##Value", &value.x);

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    void DrawMaterialSlotEditor(
        Quiet::Scene::Scene& scene,
        Quiet::Scene::Actor& actor,
        Quiet::Editor::EditorCommands& commands,
        Quiet::Scene::MaterialComponent& materialSlot,
        const std::uint32_t slotIndex,
        const std::vector<std::string>& textureAssetIDs,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const Quiet::Renderer::UI::TrackedInspectorInteraction& tracked,
        const float uiScale)
    {
        ImGui::PushID(static_cast<int>(slotIndex));

        const std::string slotLabel = "Slot " + std::to_string(slotIndex);
        if (ImGui::TreeNodeEx(slotLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec4 baseColor = materialSlot.BaseColor;
            if (ImGui::ColorEdit4("Base Color", &baseColor.x))
            {
                commands.SetStaticMeshMaterialSlotBaseColor(
                    scene,
                    actor.ID,
                    slotIndex,
                    baseColor
                );
            }

            tracked.Begin();
            tracked.End();

            const char* texturePreview =
                materialSlot.HasBaseColorTexture()
                    ? materialSlot.BaseColorTextureAssetID.c_str()
                    : "None";

            if (QUI::EditorBeginCombo("Base Color Texture", texturePreview))
            {
                const bool noneSelected = !materialSlot.HasBaseColorTexture();

                if (QUI::EditorSelectable("None", noneSelected))
                {
                    commands.SetStaticMeshMaterialSlotBaseColorTextureAsset(
                        scene,
                        actor.ID,
                        slotIndex,
                        std::string{}
                    );
                }

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                if (!textureAssetIDs.empty())
                    ImGui::Separator();

                for (const std::string& textureAssetID : textureAssetIDs)
                {
                    const bool selected = materialSlot.BaseColorTextureAssetID == textureAssetID;
                    if (QUI::EditorSelectable(textureAssetID.c_str(), selected))
                    {
                        commands.SetStaticMeshMaterialSlotBaseColorTextureAsset(
                            scene,
                            actor.ID,
                            slotIndex,
                            textureAssetID
                        );
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (!materialSlot.HasBaseColorTexture())
            {
                DrawInspectorHint("No texture assigned to this slot.");
            }
            else
            {
                DrawMaterialTexturePreviewIfAvailable(
                    materialSlot.BaseColorTextureAssetID,
                    textureLibrary,
                    uiScale,
                    96.0f
                );
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
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
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##ActorHeaderCard", ImVec2(0.0f, 100.0f), ImGuiChildFlags_Borders);
        {
            ImGui::TextUnformatted("Actor");
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("Scene Object");
            ImGui::PopStyleColor();

            char nameBuffer[256]{};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##ActorName", nameBuffer, sizeof(nameBuffer)))
            {
                commands.RenameActor(scene, actor.ID, nameBuffer);
            }

            tracked.Begin();
            tracked.End();

            const std::string actorIDText = Quiet::Core::UUIDToString(actor.ID);

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::Text("ID: %s", actorIDText.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();

        auto& transform = actor.Transform.WorldTransform;

        const auto transformHeader = BeginInspectorComponent(
            "Transform",
            "TransformComponentOptions",
            false,
            true
        );

        if (transformHeader.ResetRequested)
        {
            commands.SetActorPosition(scene, actor.ID, glm::vec3(0.0f));
            commands.SetActorRotation(scene, actor.ID, glm::vec3(0.0f));
            commands.SetActorScale(scene, actor.ID, glm::vec3(1.0f));
        }

        if (transformHeader.Open)
        {
            glm::vec3 position = transform.Position;
            if (DrawVec3PropertyRow("Position", position, 0.1f, 0.0f, 0.0f, tracked))
            {
                commands.SetActorPosition(scene, actor.ID, position);
            }

            glm::vec3 rotation = transform.RotationDegrees;
            if (DrawVec3PropertyRow("Rotation", rotation, 0.5f, 0.0f, 0.0f, tracked))
            {
                commands.SetActorRotation(scene, actor.ID, rotation);
            }

            glm::vec3 scale = transform.Scale;
            if (DrawVec3PropertyRow("Scale", scale, 0.05f, 0.01f, 100.0f, tracked))
            {
                commands.SetActorScale(scene, actor.ID, scale);
            }
        }
    }

    void DrawAddComponentMenu(Scene::Scene &scene, Scene::Actor &actor,
                              Editor::EditorCommands &commands,
                              const Project::ProjectContext &projectContext,
                              const Vulkan::MeshLibrary &meshLibrary)
    {
        const auto meshAssetIDs = projectContext.EnumerateMeshAssetIDs();
        const bool canAddStaticMesh = !actor.StaticMesh.has_value() && !meshAssetIDs.empty();
        const bool canAddMaterial = actor.StaticMesh.has_value() && !actor.Material.has_value();
        const bool canAddDirectionalLight = !actor.DirectionalLight.has_value();
        const bool canAddRotator = !actor.Rotator.has_value();
        const bool canAddCamera = !actor.Camera.has_value();

        const bool hasAnyAvailableComponent =
            canAddStaticMesh ||
            canAddMaterial ||
            canAddDirectionalLight ||
            canAddRotator ||
            canAddCamera;

        if (EditorButton("+ Add Component", ImVec2(-1.0f, 0.0f), hasAnyAvailableComponent))
            ImGui::OpenPopup("##AddComponentPopup");

        if (!hasAnyAvailableComponent)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped("All available components are already attached to this actor.");
            ImGui::PopStyleColor();
        }

        if (!actor.StaticMesh.has_value() && meshAssetIDs.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextWrapped("Static Mesh cannot be added because no mesh assets are registered.");
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopup("##AddComponentPopup"))
        {
            if (EditorMenuItem("Static Mesh", nullptr, false, canAddStaticMesh))
            {
                commands.AddStaticMesh(scene, actor.ID, projectContext, meshLibrary);
                ImGui::CloseCurrentPopup();
            }

            if (!actor.StaticMesh.has_value() && meshAssetIDs.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Register or load a mesh asset before adding a Static Mesh component.");

            if (EditorMenuItem("Material", nullptr, false, canAddMaterial))
            {
                commands.AddMaterial(scene, actor.ID);
                ImGui::CloseCurrentPopup();
            }

            if (!actor.StaticMesh.has_value() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("A Material component requires a Static Mesh component.");

            ImGui::Separator();

            if (EditorMenuItem("Directional Light", nullptr, false, canAddDirectionalLight))
            {
                commands.AddDirectionalLight(scene, actor.ID);
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Rotator", nullptr, false, canAddRotator))
            {
                commands.AddRotator(scene, actor.ID);
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Camera", nullptr, false, canAddCamera))
            {
                commands.AddCamera(scene, actor.ID);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }


    void DrawStaticMeshSection(Scene::Scene& scene,
                               Scene::Actor& actor,
                               Editor::EditorCommands& commands,
                               const Project::ProjectContext& projectContext,
                               const Vulkan::MeshLibrary& meshLibrary)
    {
        const auto meshAssetIDs = projectContext.EnumerateMeshAssetIDs();
        if (!actor.StaticMesh.has_value()) return;

        const auto header = BeginInspectorComponent("Static Mesh", "StaticMeshComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveStaticMesh(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        if (meshAssetIDs.empty())
            DrawInspectorHint("No mesh assets registered.");

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

            if (BeginComboPropertyRow("Mesh Resource", currentPreview))
            {
                for (const auto & meshAssetID : meshAssetIDs)
                {
                    const bool isSelected = (meshAssetID == actor.StaticMesh->MeshResourceID);
                    if (EditorSelectable(meshAssetID.c_str(), isSelected))
                        commands.SetStaticMeshResource(scene, actor.ID, meshAssetID, projectContext, meshLibrary);
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        bool visible = actor.StaticMesh->Visible;
        if (DrawCheckboxPropertyRow("Visible", visible))
            commands.SetStaticMeshVisible(scene, actor.ID, visible);
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
        if (!actor.Material.has_value()) return;

        const auto header = BeginInspectorComponent("Material", "MaterialComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveMaterial(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        glm::vec4 baseColor = actor.Material->BaseColor;
        if (DrawColor4PropertyRow("Base Color", baseColor, tracked))
            commands.SetMaterialBaseColor(scene, actor.ID, baseColor);

        ImGui::Spacing();

        const std::vector<std::string> textureAssetIDs =
            projectContext.EnumerateTextureAssetIDs();

        const char* texturePreview =
            actor.Material->HasBaseColorTexture()
                ? actor.Material->BaseColorTextureAssetID.c_str()
                : "None";

        if (BeginComboPropertyRow("Base Color Texture", texturePreview))
        {
            const bool noneSelected = !actor.Material->HasBaseColorTexture();
            if (EditorSelectable("None", noneSelected))
                commands.SetMaterialBaseColorTextureAsset(scene, actor.ID, std::string{});

            if (noneSelected) ImGui::SetItemDefaultFocus();
            if (!textureAssetIDs.empty()) ImGui::Separator();

            for (const std::string& textureAssetID : textureAssetIDs)
            {
                const bool selected = actor.Material->BaseColorTextureAssetID == textureAssetID;
                if (EditorSelectable(textureAssetID.c_str(), selected))
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
            DrawInspectorHint("No texture assets registered.");
        else if (!actor.Material->HasBaseColorTexture())
            DrawInspectorHint("No base color texture assigned.");
        else
        {
            const std::string& textureAssetID = actor.Material->BaseColorTextureAssetID;
            const auto textureStats = textureLibrary.GetStats(textureAssetID);

            if (!textureStats.has_value())
                DrawInspectorWarning("Assigned texture is not loaded. Press Reload in the Assets panel.");
            else if (!textureStats->GpuReady)
                DrawInspectorWarning("Assigned texture metadata is loaded, but the GPU texture is not ready.");
            else
            {
                DrawInspectorHint("Texture is sampled by the static mesh material.");

                DrawTexturePreview(
                    textureAssetID,
                    textureLibrary,
                    textureStats.value(),
                    uiScale,
                    128.0f
                );
            }
        }

        if (actor.StaticMesh.has_value() &&  !actor.StaticMesh->MaterialSlots.empty())
        {
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawInspectorHint("Imported submeshes use these slots by their source material index.");

                for (std::size_t slotIndex = 0;
                     slotIndex < actor.StaticMesh->MaterialSlots.size();
                     ++slotIndex)
                {
                    DrawMaterialSlotEditor(
                        scene,
                        actor,
                        commands,
                        actor.StaticMesh->MaterialSlots[slotIndex],
                        static_cast<std::uint32_t>(slotIndex),
                        textureAssetIDs,
                        textureLibrary,
                        tracked,
                        uiScale
                    );
                }
            }
        }
    }

    void DrawDirectionalLightSection(Scene::Scene &scene,
                                     Scene::Actor &actor,
                                     Editor::EditorCommands &commands,
                                     const TrackedInspectorInteraction &tracked)
    {
        if (!actor.DirectionalLight.has_value()) return;

        const auto header = BeginInspectorComponent("Directional Light", "DirectionalLightComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveDirectionalLight(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        bool enabled = actor.DirectionalLight->Enabled;
        if (DrawCheckboxPropertyRow("Enabled", enabled))
            commands.SetDirectionalLightEnabled(scene, actor.ID, enabled);

        glm::vec3 direction = actor.DirectionalLight->Direction;
        if (DrawVec3PropertyRow("Direction", direction, 0.01f, -1.0f, 1.0f, tracked))
            commands.SetDirectionalLightDirection(scene, actor.ID, direction);

        glm::vec3 color = actor.DirectionalLight->Color;
        if (DrawColor3PropertyRow("Color", color, tracked))
            commands.SetDirectionalLightColor(scene, actor.ID, color);

        float intensity = actor.DirectionalLight->Intensity;
        if (DrawFloatPropertyRow("Intensity", intensity, 0.01f, 0.0f, 10.0f, tracked))
            commands.SetDirectionalLightIntensity(scene, actor.ID, intensity);
    }

    void DrawRotatorSection(Scene::Scene& scene,
                            Scene::Actor& actor,
                            Editor::EditorCommands& commands,
                            const TrackedInspectorInteraction& tracked)
    {
        if (!actor.Rotator.has_value()) return;

        const auto header = BeginInspectorComponent("Rotator", "RotatorComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveRotator(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        float rotationSpeed = actor.Rotator->RotationSpeedDegreesPerSecond;
        if (DrawFloatPropertyRow("Rotation Speed", rotationSpeed, 1.0f, 0.0f, 0.0f, tracked))
            commands.SetRotatorSpeed(scene, actor.ID, rotationSpeed);
    }

    void DrawCameraSection(Scene::Scene& scene,
                           Scene::Actor& actor,
                           Editor::EditorCommands& commands,
                           Editor::EditorState& editorState,
                           const TrackedInspectorInteraction& tracked,
                           const bool isEditMode)
    {
        if (!actor.Camera.has_value()) return;

        const auto header = BeginInspectorComponent("Camera", "CameraComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveCamera(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        const bool isPilotingThisCamera =
            editorState.Selection.PilotedCameraActorID.has_value() &&
            editorState.Selection.PilotedCameraActorID.value() == actor.ID;

        if (isEditMode)
        {
            if (!isPilotingThisCamera)
            {
                if (EditorButton("Pilot Camera"))
                    editorState.Selection.PilotedCameraActorID = actor.ID;
            }
            else
            {
                if (EditorButton("Stop Piloting"))
                    editorState.Selection.PilotedCameraActorID.reset();
            }
        }

        bool primary = actor.Camera->Primary;
        if (DrawCheckboxPropertyRow("Primary", primary))
            commands.SetCameraPrimary(scene, actor.ID, primary);

        float fov = actor.Camera->FieldOfViewDegrees;
        if (DrawFloatPropertyRow("Field Of View", fov, 0.25f, 1.0f, 179.0f, tracked))
            commands.SetCameraFieldOfView(scene, actor.ID, fov);

        float nearClip = actor.Camera->NearClip;
        if (DrawFloatPropertyRow("Near Clip", nearClip, 0.01f, 0.001f, 100.0f, tracked))
            commands.SetCameraNearClip(scene, actor.ID, nearClip);

        float farClip = actor.Camera->FarClip;
        if (DrawFloatPropertyRow("Far Clip", farClip, 1.0f, 1.0f, 100000.0f, tracked))
            commands.SetCameraFarClip(scene, actor.ID, farClip);
    }
}
