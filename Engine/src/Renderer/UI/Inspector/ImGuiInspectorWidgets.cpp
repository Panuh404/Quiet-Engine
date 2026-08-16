#include "Renderer/UI/Inspector/ImGuiInspectorWidgets.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include "ImGuiInspectorSections.h"
#include "ImGuiInspectorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/UI/Assets/ImGuiAssetPicker.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"


namespace Quiet::Renderer::UI::Inspector
{
#pragma region Helpers

    void DrawInspectorHint(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    void DrawInspectorWarning(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    void DrawInspectorSubsectionHeader(const std::string& label)
    {
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopStyleColor();

        const ImVec2 labelMin = ImGui::GetItemRectMin();
        const ImVec2 labelMax = ImGui::GetItemRectMax();

        const float lineStartX = labelMax.x + EditorSizing::Px(8.0f);
        const float lineEndX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        if (lineEndX > lineStartX)
        {
            const float lineY = labelMin.y + (labelMax.y - labelMin.y) * 0.5f;

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(lineStartX, lineY),
                ImVec2(lineEndX, lineY),
                ImGui::ColorConvertFloat4ToU32(EditorColor::BorderSoft),
                EditorSizing::Px(1.0f)
            );
        }

        ImGui::Dummy(ImVec2(0.0f, EditorSizing::Px(3.0f)));
    }

    void BeginInspectorPropertyRow(const char* label)
    {
        const float rowStartX = ImGui::GetCursorPosX();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float labelWidth = std::clamp(availableWidth * 0.28f, EditorSizing::Px(92.0f), EditorSizing::Px(128.0f));

        ImGui::AlignTextToFramePadding();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::SameLine(rowStartX + labelWidth);
    }

    const char* GetRigidBodyMotionTypeLabel(const Scene::RigidBodyMotionType type)
    {
        using  Scene::RigidBodyMotionType;
        switch (type)
        {
            case RigidBodyMotionType::Static:       return "Static";
            case RigidBodyMotionType::Dynamic:      return "Dynamic";
            case RigidBodyMotionType::Kinematic:    return "Kinematic";
            default:                                return "Unknown";
        }
    }

#pragma endregion

    void DrawInspectorBanner(const char* message)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, EditorSizing::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorSizing::Px(ImVec2(10.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::AccentSoft);

        ImGui::BeginChild("##InspectorBanner", ImVec2(0.0f, EditorSizing::Px(44.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
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

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, EditorSizing::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorSizing::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##InspectorEmptyState", ImVec2(0.0f, EditorSizing::Px(92.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextUnformatted("No actor selected");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped("Select an actor from the Hierarchy or the viewport to inspect its components.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    
    void DrawFloatingComponentOptionsMenu(InspectorState& state,
                                          const char* popupID,
                                          const bool removable,
                                          const bool resettable,
                                          const bool clipboardSupported,
                                          const bool pasteAvailable,
                                          const char* pasteUnavailableReason,
                                          ComponentHeaderResult& result)
    {
        if (state.OpenComponentMenuID != popupID) return;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            state.OpenComponentMenuID.clear();
            return;
        }

        const int menuItemCount = (resettable ? 1 : 0) + (clipboardSupported ? 2 : 0) + 1;
        const int separatorCount = (resettable && clipboardSupported ? 1 : 0) + (resettable || clipboardSupported ? 1 : 0);

        const float menuWidth = EditorSizing::Px(230.0f);
        const float estimatedMenuHeight = EditorSizing::Px(
            12.0f +
            static_cast<float>(menuItemCount) * 30.0f +
            static_cast<float>(separatorCount) * 5.0f
        );

        const ImVec2 menuSize(menuWidth, estimatedMenuHeight);

        // Clamp Menu Position To Viewport
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float viewportMargin = EditorSizing::Px(8.0f);

        const float minX = viewport->WorkPos.x + viewportMargin;
        const float minY = viewport->WorkPos.y + viewportMargin;
        const float maxX = viewport->WorkPos.x + viewport->WorkSize.x - menuSize.x - viewportMargin;
        const float maxY = viewport->WorkPos.y + viewport->WorkSize.y - menuSize.y - viewportMargin;

        const ImVec2 menuPosition = {
            std::clamp(state.OpenComponentMenuPosition.x, minX, maxX),
            std::clamp(state.OpenComponentMenuPosition.y, minY, maxY)
        };

        const std::string windowID = std::string("##FloatingComponentMenu_") + popupID;
        ImGui::SetNextWindowPos(menuPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(menuWidth, 0.0f), ImGuiCond_Always);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, EditorSizing::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, EditorSizing::Px(1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorSizing::Px(ImVec2(6.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, EditorSizing::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, EditorSizing::Px(ImVec2(9.0f, 6.0f)));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColor::Background1);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::Border);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.160f, 0.055f, 0.065f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.270f, 0.080f, 0.095f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.360f, 0.095f, 0.115f, 1.0f));

        bool hovered = false;

        if (ImGui::Begin(windowID.c_str(), nullptr, flags))
        {
            hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            const ImVec2 itemSize(ImGui::GetContentRegionAvail().x, 0.0f);

            bool hasPreviousAction = false;
            if (resettable)
            {
                if (EditorSelectable("Reset Component", false, 0, itemSize))
                {
                    result.ResetRequested = true;
                    state.OpenComponentMenuID.clear();
                }
                hasPreviousAction = true;
            }

            if (clipboardSupported)
            {
                if (hasPreviousAction) ImGui::Separator();
                if (EditorSelectable("Copy Component", false, 0, itemSize))
                {
                    result.CopyRequested = true;
                    state.OpenComponentMenuID.clear();
                }

                if (EditorSelectable("Paste Component Values", false, 0, itemSize, pasteAvailable))
                {
                    result.PasteRequested = true;
                    state.OpenComponentMenuID.clear();
                }

                if (!pasteAvailable &&
                    pasteUnavailableReason &&
                    pasteUnavailableReason[0] != '\0' &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("%s", pasteUnavailableReason);
                }

                hasPreviousAction = true;
            }

            if (hasPreviousAction) ImGui::Separator();
            if (removable)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
                if (EditorSelectable("Remove Component", false, 0, itemSize))
                {
                    result.RemoveRequested = true;
                    state.OpenComponentMenuID.clear();
                }
                ImGui::PopStyleColor();
            }
            else
            {
                EditorSelectable("Transform cannot be removed", false, 0, itemSize, false);
            }
        }

        ImGui::End();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(5);

        const bool openedThisFrame = state.OpenComponentMenuFrame == ImGui::GetFrameCount();
        if (!openedThisFrame && !hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            state.OpenComponentMenuID.clear();
        }
    }

    ComponentHeaderResult BeginInspectorComponent(InspectorState& state,
                                                  const char* title,
                                                  const char* popupID,
                                                  bool removable,
                                                  bool resettable,
                                                  bool clipboardSupported,
                                                  bool pasteAvailable,
                                                  const char* pasteUnavailableReason)
    {
        ComponentHeaderResult result{};

        ImGui::PushID(popupID);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, EditorSizing::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleColor(ImGuiCol_Header, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorColor::AccentDim);

        result.Open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const float buttonSize = ImGui::GetFrameHeight();
        const float rightX = ImGui::GetWindowContentRegionMax().x - buttonSize;

        ImGui::SameLine();
        ImGui::SetCursorPosX(rightX);

        const char* gearButtonID = "##ComponentOptionsButton";
        const char* gearIcon = Icons::Gear;

        ImGui::InvisibleButton(gearButtonID, ImVec2(buttonSize, buttonSize));

        const bool gearHovered = ImGui::IsItemHovered();
        const bool gearPressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (gearHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const ImVec2 gearMin = ImGui::GetItemRectMin();
        const ImVec2 gearMax = ImGui::GetItemRectMax();

        const ImVec2 iconSize = ImGui::CalcTextSize(gearIcon);
        const ImVec2 iconPos = ImVec2(gearMin.x + (buttonSize - iconSize.x) * 0.5f, gearMin.y + (buttonSize - iconSize.y) * 0.5f);
        const ImVec4 iconColor = gearHovered ? EditorColor::AccentHover : EditorColor::TextMuted;

        ImGui::GetWindowDrawList()->AddText(iconPos, ImGui::ColorConvertFloat4ToU32(iconColor), gearIcon);

        if (gearPressed)
        {
            if (state.OpenComponentMenuID == popupID)
            {
                state.OpenComponentMenuID.clear();
            }
            else
            {
                const float menuWidth = EditorSizing::Px(230.0f);
                state.OpenComponentMenuID = popupID;
                state.OpenComponentMenuFrame = ImGui::GetFrameCount();
                state.OpenComponentMenuPosition = ImVec2(gearMax.x - menuWidth, gearMax.y + EditorSizing::Px(4.0f));
            }
        }
        else if (state.OpenComponentMenuID == popupID)
        {
            const float menuWidth = EditorSizing::Px(230.0f);
            state.OpenComponentMenuPosition = ImVec2(gearMax.x - menuWidth, gearMax.y + EditorSizing::Px(4.0f));
        }

        ImGui::PopID();
        DrawFloatingComponentOptionsMenu(state, popupID, removable, resettable, clipboardSupported, pasteAvailable, pasteUnavailableReason, result);
        return result;
    }

    bool DrawCheckboxPropertyRow(const char* label, bool& value)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        const bool changed = EditorCheckbox("##Value", &value);

        ImGui::PopID();
        return changed;
    }

    bool DrawRigidBodyMotionTypePropertyRow(const char* label, Scene::RigidBodyMotionType& value)
    {
        using Scene::RigidBodyMotionType;

        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const char* preview = GetRigidBodyMotionTypeLabel(value);
        bool changed = false;

        if (EditorBeginCombo("##Value", preview))
        {
            const auto drawOption = [&](const char* optionLabel, const RigidBodyMotionType option)
            {
                const bool selected = value == option;
                if (EditorSelectable(optionLabel, selected))
                {
                    value = option;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            };

            drawOption("Static", RigidBodyMotionType::Static);
            drawOption("Dynamic", RigidBodyMotionType::Dynamic);
            drawOption("Kinematic", RigidBodyMotionType::Kinematic);
            ImGui::EndCombo();
        }

        ImGui::PopID();
        return changed;
    }

    bool DrawAssetPickerPropertyRow(const char* label,
                                    const Project::ProjectContext& projectContext,
                                    const Assets::AssetPickerOptions& options,
                                    const std::string& currentAssetID,
                                    std::string& outSelectedAssetID)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        const bool changed = Assets::DrawAssetPicker("##Value", projectContext, options, currentAssetID, outSelectedAssetID);
        ImGui::PopID();

        return changed;
    }

    bool DrawFloatPropertyRow(const char* label, float& value, const float speed, const float minValue, const float maxValue, const TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const ImGuiSliderFlags flags = minValue < maxValue ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
        const bool changed = ImGui::DragFloat("##Value", &value, speed, minValue, maxValue, "%.3f", flags);
        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool DrawVec3PropertyRow(const char* label, glm::vec3& value, const float speed, const float minValue, const float maxValue, const TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginInspectorPropertyRow(label);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float axisLabelWidth = EditorSizing::Px(14.0f);
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        const float fieldWidth = std::max(EditorSizing::Px(54.0f), (availableWidth - axisLabelWidth * 3.0f - spacing * 5.0f) / 3.0f);

        bool changed = false;
        auto drawAxis = [&](const char* axisLabel, float& axisValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(axisLabel);
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::SetNextItemWidth(fieldWidth);
            const ImGuiSliderFlags flags = minValue < maxValue ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;

            changed |= ImGui::DragFloat("##Axis", &axisValue, speed, minValue, maxValue, "%.3f", flags);
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

    bool DrawColor3PropertyRow(const char* label, glm::vec3& value, const TrackedInspectorInteraction& tracked)
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

    bool DrawColor4PropertyRow(const char* label, glm::vec4& value, const TrackedInspectorInteraction& tracked)
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

    std::string GetMaterialSlotSourceName(const Scene::Actor& actor, const std::uint32_t slotIndex)
    {
        const auto* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        if (!staticMesh || !staticMesh->Mesh) return {};

        const auto& importedSlots = staticMesh->Mesh->GetImportedMaterialSlots();
        if (slotIndex >= importedSlots.size()) return {};

        return importedSlots[slotIndex].Name;
    }

    std::size_t CountSubmeshesUsingMaterialSlot(const Scene::Actor& actor, const std::uint32_t slotIndex)
    {
        const auto* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        if (!staticMesh || !staticMesh->Mesh) return 0;

        const auto& submeshes = staticMesh->Mesh->GetSubmeshes();

        return static_cast<std::size_t>(
            std::count_if(
                submeshes.begin(),
                submeshes.end(),
                [slotIndex](const Vulkan::StaticSubmesh& submesh)
                {
                    return submesh.MaterialSlot == slotIndex;
                }
            )
        );
    }

    std::string MakeMaterialSlotUsageLabel(const std::size_t submeshCount)
    {
        if (submeshCount == 1) return "1 submesh";
        return std::to_string(submeshCount) + " submeshes";
    }

    void DrawCompactMaterialTexturePreview(const std::string& textureAssetID, const Vulkan::TextureLibrary& textureLibrary)
    {
        if (textureAssetID.empty()) return;
        const auto textureStats = textureLibrary.GetStats(textureAssetID);
        if (!textureStats.has_value())
        {
            DrawInspectorWarning("Texture is not loaded. Press Reload in the Assets panel." );
            return;
        }
        if (!textureStats->GpuReady)
        {
            DrawInspectorWarning("Texture metadata is loaded, but the GPU texture is not ready.");
            return;
        }

        const ImTextureID previewID = GetOrCreateTexturePreviewID(textureAssetID, textureLibrary);
        if (!previewID)
        {
            DrawInspectorWarning("Texture preview is unavailable.");
            return;
        }

        BeginInspectorPropertyRow("Preview");

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float frameSide = std::clamp(availableWidth, EditorSizing::Px(44.0f), EditorSizing::Px(76.0f));
        const float framePadding = EditorSizing::Px(6.0f);

        const float maxImageSide = std::max(EditorSizing::Px(1.0f), frameSide - framePadding * 2.0f);
        const ImVec2 imageSize = CalculateTexturePreviewSize(textureStats->Width, textureStats->Height, maxImageSide);

        const ImVec2 frameMin = ImGui::GetCursorScreenPos();
        const ImVec2 frameMax(frameMin.x + frameSide, frameMin.y + frameSide);

        const ImVec2 imageMin(
            frameMin.x + (frameSide - imageSize.x) * 0.5f,
            frameMin.y + (frameSide - imageSize.y) * 0.5f);

        const ImVec2 imageMax(
            imageMin.x + imageSize.x,
            imageMin.y + imageSize.y
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            frameMin,
            frameMax,
            ImGui::ColorConvertFloat4ToU32(EditorColor::Background1),
            EditorSizing::Px(4.0f)
        );

        drawList->AddRect(
            frameMin,
            frameMax,
            ImGui::ColorConvertFloat4ToU32(EditorColor::Border),
            EditorSizing::Px(4.0f),
            0,
            EditorSizing::Px(1.0f)
        );

        // Texture pixels are flipped during import to match mesh UVs.
        drawList->AddImage(
            previewID,
            imageMin,
            imageMax,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f),
            IM_COL32_WHITE
        );

        ImGui::Dummy(ImVec2(frameSide, frameSide));
    }

    bool DrawMaterialSlotHeader(const Scene::Actor& actor, const Scene::MaterialComponent& materialSlot, const std::uint32_t slotIndex, const bool defaultOpen)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
        if (defaultOpen)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, EditorSizing::Px(ImVec2(8.0f, 7.0f)));

        ImGui::PushStyleColor(ImGuiCol_Header, EditorColor::Background3);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorColor::AccentDim);

        const bool open = ImGui::TreeNodeEx("##MaterialSlotHeader", flags);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float headerHeight = headerMax.y - headerMin.y;

        const float swatchSize = std::max(EditorSizing::Px(12.0f), std::min(EditorSizing::Px(17.0f), headerHeight - EditorSizing::Px(8.0f)));

        const float labelStartX = headerMin.x + ImGui::GetTreeNodeToLabelSpacing();

        const ImVec2 swatchMin(labelStartX, headerMin.y + (headerHeight - swatchSize) * 0.5f);
        const ImVec2 swatchMax(swatchMin.x + swatchSize, swatchMin.y + swatchSize);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            swatchMin,
            swatchMax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(materialSlot.BaseColor.r, materialSlot.BaseColor.g, materialSlot.BaseColor.b, materialSlot.BaseColor.a)),
            EditorSizing::Px(3.0f)
        );

        drawList->AddRect(
            swatchMin,
            swatchMax,
            ImGui::ColorConvertFloat4ToU32(EditorColor::Border),
            EditorSizing::Px(3.0f),
            0,
            EditorSizing::Px(1.0f)
        );

        const float textY = headerMin.y + (headerHeight - ImGui::GetTextLineHeight()) * 0.5f;
        const float titleX = swatchMax.x + EditorSizing::Px(8.0f);

        const std::string slotTitle = "Slot " + std::to_string(slotIndex);

        drawList->AddText(
            ImVec2(titleX, textY),
            ImGui::ColorConvertFloat4ToU32(EditorColor::Text),
            slotTitle.c_str()
        );

        const std::size_t submeshCount = CountSubmeshesUsingMaterialSlot(actor, slotIndex);

        const std::string metadata = MakeMaterialSlotUsageLabel(submeshCount) + " | " + (materialSlot.HasBaseColorTexture() ? "Texture" : "Color only");
        const float metadataWidth = ImGui::CalcTextSize(metadata.c_str()).x;
        const float metadataX = headerMax.x - metadataWidth - EditorSizing::Px(10.0f);

        const bool showMetadata = metadataX > titleX + EditorSizing::Px(110.0f);
        if (showMetadata)
        {
            drawList->AddText(
                ImVec2(metadataX, textY),
                ImGui::ColorConvertFloat4ToU32(EditorColor::TextDim),
                metadata.c_str()
            );
        }

        const std::string sourceName = GetMaterialSlotSourceName(actor, slotIndex);
        if (!sourceName.empty())
        {
            const float titleWidth = ImGui::CalcTextSize(slotTitle.c_str()).x;

            const float sourceNameX = titleX + titleWidth + EditorSizing::Px(10.0f);
            const float sourceNameMaxX = showMetadata
                ? metadataX - EditorSizing::Px(10.0f)
                : headerMax.x - EditorSizing::Px(10.0f);

            if (sourceNameMaxX > sourceNameX)
            {
                drawList->PushClipRect(
                    ImVec2(sourceNameX, headerMin.y),
                    ImVec2(sourceNameMaxX, headerMax.y),
                    true
                );

                drawList->AddText(
                    ImVec2(sourceNameX, textY),
                    ImGui::ColorConvertFloat4ToU32(EditorColor::TextMuted),
                    sourceName.c_str()
                );

                drawList->PopClipRect();
            }
        }

        return open;
    }

    void DrawMaterialSlotEditor(Scene::Scene& scene,
                                Scene::Actor& actor,
                                Editor::EditorCommands& commands,
                                const Project::ProjectContext& projectContext,
                                Scene::MaterialComponent& materialSlot,
                                const std::uint32_t slotIndex,
                                const Vulkan::TextureLibrary& textureLibrary,
                                const TrackedInspectorInteraction& tracked,
                                const bool hasTextureAssets,
                                const bool defaultOpen)
    {
        ImGui::PushID(static_cast<int>(slotIndex));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, EditorSizing::Px(7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EditorSizing::Px(ImVec2(9.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        ImGui::BeginChild("##MaterialSlotCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
            ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const bool open = DrawMaterialSlotHeader(actor, materialSlot, slotIndex, defaultOpen);
        if (open)
        {
            ImGui::Spacing();

            glm::vec4 baseColor = materialSlot.BaseColor;
            if (DrawColor4PropertyRow("Base Color", baseColor, tracked))
            {
                commands.SetStaticMeshMaterialSlotBaseColor(scene, actor.GetID(), slotIndex, baseColor);
            }

            Assets::AssetPickerOptions texturePickerOptions{};
            texturePickerOptions.Type = Quiet::Project::AssetType::Texture;
            texturePickerOptions.AllowNone = true;
            texturePickerOptions.TextureLibrary = &textureLibrary;

            std::string selectedTextureAssetID;
            if (DrawAssetPickerPropertyRow("Base Color Texture", projectContext, texturePickerOptions, materialSlot.BaseColorTextureAssetID, selectedTextureAssetID))
            {
                commands.SetStaticMeshMaterialSlotBaseColorTextureAsset(scene, actor.GetID(), slotIndex, selectedTextureAssetID);
            }

            if (!materialSlot.HasBaseColorTexture())
            {
                if (!hasTextureAssets)
                {
                    DrawInspectorHint("No texture assets are registered.");
                }
                else
                {
                    DrawInspectorHint("This slot uses its base color only.");
                }
            }
            else
            {
                DrawCompactMaterialTexturePreview(materialSlot.BaseColorTextureAssetID, textureLibrary);
            }

            ImGui::TreePop();
        }

        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Dummy(ImVec2(0.0f, EditorSizing::Px(5.0f)));

        ImGui::PopID();
    }

}