#include "ImGuiInspectorSections.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/UI/Assets/ImGuiAssetPicker.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace ASSETS = Quiet::Renderer::UI::Assets;

    enum class InspectorComponentClipboardType
    {
        None,
        Transform,
        StaticMesh,
        Material,
        DirectionalLight,
        Rotator,
        Camera
    };

    struct InspectorComponentClipboardState
    {
        InspectorComponentClipboardType Type = InspectorComponentClipboardType::None;
        Quiet::Scene::TransformComponent Transform{};

        std::string StaticMeshResourceID;
        bool StaticMeshVisible = true;

        Quiet::Scene::MaterialComponent Material{};
        std::vector<Quiet::Scene::MaterialComponent> MaterialSlots;

        Quiet::Scene::DirectionalLightComponent DirectionalLight{};
        Quiet::Scene::RotatorComponent Rotator{};
        Quiet::Scene::CameraComponent Camera{};
    };

    InspectorComponentClipboardState g_InspectorComponentClipboard{};

    struct InspectorComponentHeaderResult
    {
        bool Open = false;
        bool RemoveRequested = false;
        bool ResetRequested = false;
        bool CopyRequested = false;
        bool PasteRequested = false;
    };

    std::string g_OpenInspectorComponentMenuID;
    ImVec2 g_OpenInspectorComponentMenuPosition = ImVec2(0.0f, 0.0f);
    int g_OpenInspectorComponentMenuFrame = -1;

    ImVec2 ClampMenuPositionToViewport(const ImVec2 desiredPosition, const ImVec2 menuSize)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float viewportMargin = SZ::Px(8.0f);

        const float minX = viewport->WorkPos.x + viewportMargin;
        const float minY = viewport->WorkPos.y + viewportMargin;
        const float maxX = viewport->WorkPos.x + viewport->WorkSize.x - menuSize.x - viewportMargin;
        const float maxY = viewport->WorkPos.y + viewport->WorkSize.y - menuSize.y - viewportMargin;

        return {std::clamp(desiredPosition.x, minX, maxX), std::clamp(desiredPosition.y, minY, maxY)};
    }

    void DrawFloatingComponentOptionsMenu(const char* popupID,
                                          const bool removable,
                                          const bool resettable,
                                          const bool clipboardSupported,
                                          const bool pasteAvailable,
                                          const char* pasteUnavailableReason,
                                          InspectorComponentHeaderResult& result)
    {
        if (g_OpenInspectorComponentMenuID != popupID) return;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            g_OpenInspectorComponentMenuID.clear();
            return;
        }

        const int menuItemCount = (resettable ? 1 : 0) + (clipboardSupported ? 2 : 0) + 1;
        const int separatorCount = (resettable && clipboardSupported ? 1 : 0) + (resettable || clipboardSupported ? 1 : 0);

        const float menuWidth = SZ::Px(230.0f);
        const float estimatedMenuHeight = SZ::Px(
            12.0f +
            static_cast<float>(menuItemCount) * 30.0f +
            static_cast<float>(separatorCount) * 5.0f
        );

        const ImVec2 menuSize(menuWidth, estimatedMenuHeight);
        const ImVec2 menuPosition = ClampMenuPositionToViewport(g_OpenInspectorComponentMenuPosition, menuSize);

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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SZ::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, SZ::Px(1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(6.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SZ::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(9.0f, 6.0f)));

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

            bool hasPreviousAction = false;
            if (resettable)
            {
                if (QUI::EditorSelectable("Reset Component", false, 0, itemSize))
                {
                    result.ResetRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }
                hasPreviousAction = true;
            }

            if (clipboardSupported)
            {
                if (hasPreviousAction) ImGui::Separator();
                if (QUI::EditorSelectable("Copy Component", false, 0, itemSize))
                {
                    result.CopyRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }

                if (QUI::EditorSelectable("Paste Component Values", false, 0, itemSize, pasteAvailable))
                {
                    result.PasteRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
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
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);
                if (QUI::EditorSelectable("Remove Component", false, 0, itemSize))
                {
                    result.RemoveRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }
                ImGui::PopStyleColor();
            }
            else
            {
                QUI::EditorSelectable("Transform cannot be removed", false, 0, itemSize, false);
            }
        }

        ImGui::End();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(5);

        const bool openedThisFrame = g_OpenInspectorComponentMenuFrame == ImGui::GetFrameCount();
        if (!openedThisFrame && !hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            g_OpenInspectorComponentMenuID.clear();
        }
    }

    InspectorComponentHeaderResult BeginInspectorComponent(const char* title,
                                                           const char* popupID,
                                                           const bool removable = true,
                                                           const bool resettable = false,
                                                           const bool clipboardSupported = false,
                                                           const bool pasteAvailable = false,
                                                           const char* pasteUnavailableReason = nullptr)
    {
        InspectorComponentHeaderResult result{};

        ImGui::PushID(popupID);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleColor(ImGuiCol_Header, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, QUI::EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, QUI::EditorColor::AccentDim);

        result.Open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

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
        const ImVec2 iconPos = ImVec2(gearMin.x + (buttonSize - iconSize.x) * 0.5f, gearMin.y + (buttonSize - iconSize.y) * 0.5f);
        const ImVec4 iconColor = gearHovered ? QUI::EditorColor::AccentHover : QUI::EditorColor::TextMuted;

        ImGui::GetWindowDrawList()->AddText(iconPos, ImGui::ColorConvertFloat4ToU32(iconColor), gearIcon);

        if (gearPressed)
        {
            if (g_OpenInspectorComponentMenuID == popupID)
            {
                g_OpenInspectorComponentMenuID.clear();
            }
            else
            {
                const float menuWidth = SZ::Px(230.0f);
                g_OpenInspectorComponentMenuID = popupID;
                g_OpenInspectorComponentMenuFrame = ImGui::GetFrameCount();
                g_OpenInspectorComponentMenuPosition = ImVec2(gearMax.x - menuWidth, gearMax.y + SZ::Px(4.0f));
            }
        }
        else if (g_OpenInspectorComponentMenuID == popupID)
        {
            const float menuWidth = SZ::Px(230.0f);
            g_OpenInspectorComponentMenuPosition = ImVec2(gearMax.x - menuWidth, gearMax.y + SZ::Px(4.0f));
        }

        ImGui::PopID();
        DrawFloatingComponentOptionsMenu(popupID, removable, resettable, clipboardSupported, pasteAvailable, pasteUnavailableReason, result);
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

    void DrawInspectorSubsectionHeader(const std::string& label)
    {
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopStyleColor();

        const ImVec2 labelMin = ImGui::GetItemRectMin();
        const ImVec2 labelMax = ImGui::GetItemRectMax();

        const float lineStartX = labelMax.x + SZ::Px(8.0f);
        const float lineEndX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        if (lineEndX > lineStartX)
        {
            const float lineY = labelMin.y + (labelMax.y - labelMin.y) * 0.5f;

            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(lineStartX, lineY),
                ImVec2(lineEndX, lineY),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
                SZ::Px(1.0f)
            );
        }

        ImGui::Dummy(ImVec2(0.0f, SZ::Px(3.0f)));
    }

    float GetInspectorPropertyLabelWidth()
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        return std::clamp(availableWidth * 0.28f, SZ::Px(92.0f), SZ::Px(128.0f));
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

    bool DrawFloatPropertyRow(const char* label, float& value, const float speed, const float minValue, const float maxValue, const QUI::TrackedInspectorInteraction& tracked)
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

    bool DrawVec3PropertyRow(const char* label, glm::vec3& value, const float speed, const float minValue, const float maxValue, const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginInspectorPropertyRow(label);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float axisLabelWidth = SZ::Px(14.0f);
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        const float fieldWidth = std::max(SZ::Px(54.0f), (availableWidth - axisLabelWidth * 3.0f - spacing * 5.0f) / 3.0f);

        bool changed = false;
        auto drawAxis = [&](const char* axisLabel, float& axisValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
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

    bool DrawCheckboxPropertyRow(const char* label, bool& value)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        const bool changed = QUI::EditorCheckbox("##Value", &value);

        ImGui::PopID();
        return changed;
    }

    bool DrawAssetPickerPropertyRow(const char* label,
                                    const Quiet::Project::ProjectContext& projectContext,
                                    const ASSETS::AssetPickerOptions& options,
                                    const std::string& currentAssetID,
                                    std::string& outSelectedAssetID)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        const bool changed = ASSETS::DrawAssetPicker("##Value", projectContext, options, currentAssetID, outSelectedAssetID);
        ImGui::PopID();

        return changed;
    }

    bool DrawColor3PropertyRow(const char* label, glm::vec3& value, const QUI::TrackedInspectorInteraction& tracked)
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

    bool DrawColor4PropertyRow(const char* label, glm::vec4& value, const QUI::TrackedInspectorInteraction& tracked)
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

    std::string GetMaterialSlotSourceName(const Quiet::Scene::Actor& actor, const std::uint32_t slotIndex)
    {
        const auto* staticMesh = actor.TryGetComponent<Quiet::Scene::StaticMeshComponent>();
        if (!staticMesh || !staticMesh->Mesh) return {};

        const auto& importedSlots = staticMesh->Mesh->GetImportedMaterialSlots();
        if (slotIndex >= importedSlots.size()) return {};

        return importedSlots[slotIndex].Name;
    }

    std::size_t CountSubmeshesUsingMaterialSlot(const Quiet::Scene::Actor& actor, const std::uint32_t slotIndex)
    {
        const auto* staticMesh = actor.TryGetComponent<Quiet::Scene::StaticMeshComponent>();
        if (!staticMesh || !staticMesh->Mesh) return 0;

        const auto& submeshes = staticMesh->Mesh->GetSubmeshes();

        return static_cast<std::size_t>(
            std::count_if(
                submeshes.begin(),
                submeshes.end(),
                [slotIndex](const Quiet::Renderer::Vulkan::StaticSubmesh& submesh)
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

    void DrawCompactMaterialTexturePreview(const std::string& textureAssetID, const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary)
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

        const ImTextureID previewID = QUI::GetOrCreateTexturePreviewID(textureAssetID, textureLibrary);
        if (!previewID)
        {
            DrawInspectorWarning("Texture preview is unavailable.");
            return;
        }

        BeginInspectorPropertyRow("Preview");

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float frameSide = std::clamp(availableWidth, SZ::Px(44.0f), SZ::Px(76.0f));
        const float framePadding = SZ::Px(6.0f);

        const float maxImageSide = std::max(SZ::Px(1.0f), frameSide - framePadding * 2.0f);
        const ImVec2 imageSize = QUI::CalculateTexturePreviewSize(textureStats->Width, textureStats->Height, maxImageSide);

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
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background1),
            SZ::Px(4.0f)
        );

        drawList->AddRect(
            frameMin,
            frameMax,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Border),
            SZ::Px(4.0f),
            0,
            SZ::Px(1.0f)
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

    bool DrawMaterialSlotHeader(const Quiet::Scene::Actor& actor, const Quiet::Scene::MaterialComponent& materialSlot, const std::uint32_t slotIndex, const bool defaultOpen)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
        if (defaultOpen)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 7.0f)));

        ImGui::PushStyleColor(ImGuiCol_Header, QUI::EditorColor::Background3);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, QUI::EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, QUI::EditorColor::AccentDim);

        const bool open = ImGui::TreeNodeEx("##MaterialSlotHeader", flags);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float headerHeight = headerMax.y - headerMin.y;

        const float swatchSize = std::max(SZ::Px(12.0f), std::min(SZ::Px(17.0f), headerHeight - SZ::Px(8.0f)));

        const float labelStartX = headerMin.x + ImGui::GetTreeNodeToLabelSpacing();

        const ImVec2 swatchMin(labelStartX, headerMin.y + (headerHeight - swatchSize) * 0.5f);
        const ImVec2 swatchMax(swatchMin.x + swatchSize, swatchMin.y + swatchSize);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            swatchMin,
            swatchMax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(materialSlot.BaseColor.r, materialSlot.BaseColor.g, materialSlot.BaseColor.b, materialSlot.BaseColor.a)),
            SZ::Px(3.0f)
        );

        drawList->AddRect(
            swatchMin,
            swatchMax,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Border),
            SZ::Px(3.0f),
            0,
            SZ::Px(1.0f)
        );

        const float textY = headerMin.y + (headerHeight - ImGui::GetTextLineHeight()) * 0.5f;
        const float titleX = swatchMax.x + SZ::Px(8.0f);

        const std::string slotTitle = "Slot " + std::to_string(slotIndex);

        drawList->AddText(
            ImVec2(titleX, textY),
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Text),
            slotTitle.c_str()
        );

        const std::size_t submeshCount = CountSubmeshesUsingMaterialSlot(actor, slotIndex);

        const std::string metadata = MakeMaterialSlotUsageLabel(submeshCount) + " | " + (materialSlot.HasBaseColorTexture() ? "Texture" : "Color only");
        const float metadataWidth = ImGui::CalcTextSize(metadata.c_str()).x;
        const float metadataX = headerMax.x - metadataWidth - SZ::Px(10.0f);

        const bool showMetadata = metadataX > titleX + SZ::Px(110.0f);
        if (showMetadata)
        {
            drawList->AddText(
                ImVec2(metadataX, textY),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextDim),
                metadata.c_str()
            );
        }

        const std::string sourceName = GetMaterialSlotSourceName(actor, slotIndex);
        if (!sourceName.empty())
        {
            const float titleWidth = ImGui::CalcTextSize(slotTitle.c_str()).x;

            const float sourceNameX = titleX + titleWidth + SZ::Px(10.0f);
            const float sourceNameMaxX = showMetadata
                ? metadataX - SZ::Px(10.0f)
                : headerMax.x - SZ::Px(10.0f);

            if (sourceNameMaxX > sourceNameX)
            {
                drawList->PushClipRect(
                    ImVec2(sourceNameX, headerMin.y),
                    ImVec2(sourceNameMaxX, headerMax.y),
                    true
                );

                drawList->AddText(
                    ImVec2(sourceNameX, textY),
                    ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                    sourceName.c_str()
                );

                drawList->PopClipRect();
            }
        }

        return open;
    }

    void DrawMaterialSlotEditor(
        Quiet::Scene::Scene& scene,
        Quiet::Scene::Actor& actor,
        Quiet::Editor::EditorCommands& commands,
        const Quiet::Project::ProjectContext& projectContext,
        Quiet::Scene::MaterialComponent& materialSlot,
        const std::uint32_t slotIndex,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const Quiet::Renderer::UI::TrackedInspectorInteraction& tracked,
        const bool hasTextureAssets,
        const bool defaultOpen)
    {
        ImGui::PushID(static_cast<int>(slotIndex));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(9.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

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

            ASSETS::AssetPickerOptions texturePickerOptions{};
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

        ImGui::Dummy(ImVec2(0.0f, SZ::Px(5.0f)));

        ImGui::PopID();
    }
}

namespace Quiet::Renderer::UI
{
    void TrackedInspectorInteraction::Begin() const
    {
        if (Enabled && ImGui::IsItemActivated()) Commands.BeginInteraction(SceneRef);
    }

    void TrackedInspectorInteraction::End() const
    {
        if (Enabled && ImGui::IsItemDeactivated()) Commands.EndInteraction(SceneRef);
    }

    void DrawActorDetailsSection(Scene::Scene& scene, Scene::Actor& actor, Editor::EditorCommands& commands, const TrackedInspectorInteraction& tracked)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##ActorHeaderCard", ImVec2(0.0f, SZ::Px(100.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::TextUnformatted("Actor");
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("Scene Object");
            ImGui::PopStyleColor();

            char nameBuffer[256]{};

            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.GetName().c_str());

            ImGui::SetNextItemWidth(-1.0f);

            const bool nameChanged = ImGui::InputText("##ActorName", nameBuffer, sizeof(nameBuffer));
            tracked.Begin();
            if (nameChanged)
            {
                commands.RenameActor(scene, actor.GetID(), nameBuffer);
            }
            tracked.End();

            const std::string actorIDText = Quiet::Core::UUIDToString(actor.GetID());

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::Text("ID: %s", actorIDText.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();

        const bool canPasteTransform = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::Transform;
        const auto transformHeader =
            BeginInspectorComponent(
                "Transform",
                "TransformComponentOptions",
                false,
                true,
                true,
                canPasteTransform,
                canPasteTransform ? nullptr : "Copy Transform values first."
            );

        Scene::TransformComponent& transformComponent = actor.GetComponent<Scene::TransformComponent>();
        if (transformHeader.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::Transform;
            g_InspectorComponentClipboard.Transform = transformComponent;
        }

        if (transformHeader.PasteRequested && canPasteTransform)
        {
            commands.PasteActorTransformValues(scene, actor.GetID(), g_InspectorComponentClipboard.Transform);
        }

        if (transformHeader.ResetRequested)
        {
            commands.ResetActorTransform(scene, actor.GetID());
        }

        if (transformHeader.Open)
        {
            auto& transform = transformComponent.WorldTransform;

            glm::vec3 position = transform.Position;
            if (DrawVec3PropertyRow("Position", position, 0.1f, 0.0f, 0.0f, tracked))
            {
                commands.SetActorPosition(scene, actor.GetID(), position);
            }

            glm::vec3 rotation = transform.RotationDegrees;
            if (DrawVec3PropertyRow("Rotation", rotation, 0.5f, 0.0f, 0.0f, tracked))
            {
                commands.SetActorRotation(scene, actor.GetID(), rotation);
            }

            glm::vec3 scale = transform.Scale;
            if (DrawVec3PropertyRow("Scale", scale, 0.05f, 0.01f, 100.0f, tracked))
            {
                commands.SetActorScale(scene, actor.GetID(), scale);
            }
        }
    }

    void DrawAddComponentMenu(Scene::Scene& scene,
                              Scene::Actor& actor,
                              Editor::EditorCommands& commands,
                              const Project::ProjectContext& projectContext,
                              const Vulkan::MeshLibrary& meshLibrary)
    {
        const auto meshAssetIDs = projectContext.EnumerateMeshAssetIDs();
        const bool hasStaticMesh = actor.HasComponent<Scene::StaticMeshComponent>();

        const bool canAddStaticMesh = !hasStaticMesh && !meshAssetIDs.empty();
        const bool canAddMaterial = hasStaticMesh && !actor.HasComponent<Scene::MaterialComponent>();
        const bool canAddDirectionalLight = !actor.HasComponent<Scene::DirectionalLightComponent>();
        const bool canAddRotator = !actor.HasComponent<Scene::RotatorComponent>();
        const bool canAddCamera = !actor.HasComponent<Scene::CameraComponent>();

        const bool hasAnyAvailableComponent = canAddStaticMesh || canAddMaterial || canAddDirectionalLight || canAddRotator || canAddCamera;

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
                commands.AddStaticMesh(scene, actor.GetID(), projectContext, meshLibrary);
                ImGui::CloseCurrentPopup();
            }

            if (!hasStaticMesh && meshAssetIDs.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Register or load a mesh asset before adding a Static Mesh component.");
            }

            if (EditorMenuItem("Material", nullptr, false, canAddMaterial))
            {
                commands.AddMaterial(scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (!hasStaticMesh && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("A Material component requires a Static Mesh component.");
            }

            ImGui::Separator();

            if (EditorMenuItem("Directional Light", nullptr, false, canAddDirectionalLight))
            {
                commands.AddDirectionalLight(scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Rotator", nullptr, false, canAddRotator))
            {
                commands.AddRotator(scene, actor.GetID());
                ImGui::CloseCurrentPopup();
            }

            if (EditorMenuItem("Camera", nullptr, false, canAddCamera))
            {
                commands.AddCamera(scene, actor.GetID());
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
        Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        if (!staticMesh) return;

        const bool hasMeshAssets = !projectContext.EnumerateMeshAssetIDs().empty();
        const bool hasStaticMeshClipboard = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::StaticMesh;

        const Project::AssetRecord* copiedMeshRecord = nullptr;
        if (hasStaticMeshClipboard && !g_InspectorComponentClipboard.StaticMeshResourceID.empty())
        {
            copiedMeshRecord = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), g_InspectorComponentClipboard.StaticMeshResourceID);
        }

        const bool copiedMeshIsRegistered = copiedMeshRecord != nullptr && copiedMeshRecord->Type == Project::AssetType::Mesh;
        const bool copiedMeshIsLoaded = copiedMeshIsRegistered && meshLibrary.TryGet(g_InspectorComponentClipboard.StaticMeshResourceID) != nullptr;
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
            "Static Mesh",
            "StaticMeshComponentOptions",
            true,
            false,
            true,
            canPasteStaticMesh,
            pasteUnavailableReason);

        if (header.RemoveRequested)
        {
            commands.RemoveStaticMesh(scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::StaticMesh;
            g_InspectorComponentClipboard.StaticMeshResourceID = staticMesh->MeshResourceID;
            g_InspectorComponentClipboard.StaticMeshVisible = staticMesh->Visible;
        }

        if (header.PasteRequested && canPasteStaticMesh)
        {
            commands.PasteStaticMeshValues(
                scene,
                actor.GetID(),
                g_InspectorComponentClipboard.StaticMeshResourceID,
                g_InspectorComponentClipboard.StaticMeshVisible,
                projectContext,
                meshLibrary
            );
        }

        if (!header.Open) return;

        Assets::AssetPickerOptions meshPickerOptions{};
        meshPickerOptions.Type = Project::AssetType::Mesh;
        meshPickerOptions.MeshLibrary = &meshLibrary;

        std::string selectedMeshAssetID;

        if (DrawAssetPickerPropertyRow("Mesh Resource", projectContext, meshPickerOptions, staticMesh->MeshResourceID, selectedMeshAssetID))
        {
            commands.SetStaticMeshResource(scene, actor.GetID(), selectedMeshAssetID, projectContext, meshLibrary);
        }

        if (!hasMeshAssets)
        {
            DrawInspectorHint("No mesh assets registered.");
        }

        bool visible = staticMesh->Visible;
        if (DrawCheckboxPropertyRow("Visible", visible))
        {
            commands.SetStaticMeshVisible(scene, actor.GetID(), visible);
        }
    }

    void DrawMaterialSection(Scene::Scene& scene,
                             Scene::Actor& actor,
                             Editor::EditorCommands& commands,
                             const Project::ProjectContext& projectContext,
                             const Vulkan::TextureLibrary& textureLibrary,
                             const TrackedInspectorInteraction& tracked,
                             const float uiScale)
    {
        Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
        Scene::MaterialComponent* material = actor.TryGetComponent<Scene::MaterialComponent>();
        if (!staticMesh || !material) return;

        const bool canResetMaterial = staticMesh->Mesh != nullptr;
        const bool hasMaterialClipboard = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::Material;
        const bool materialSlotCountsMatch = hasMaterialClipboard && g_InspectorComponentClipboard.MaterialSlots.size() == staticMesh->MaterialSlots.size();

        const auto& assetRegistry = projectContext.GetAssetRegistry();
        const auto isRegisteredTextureAsset = [&](const std::string& textureAssetID)
        {
            if (textureAssetID.empty()) return true;
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(assetRegistry, textureAssetID);
            return record && record->Type == Project::AssetType::Texture;
        };

        bool copiedMaterialTexturesValid = true;

        if (hasMaterialClipboard)
        {
            copiedMaterialTexturesValid = isRegisteredTextureAsset(g_InspectorComponentClipboard.Material.BaseColorTextureAssetID);
            if (copiedMaterialTexturesValid)
            {
                for (const Scene::MaterialComponent& materialSlot : g_InspectorComponentClipboard.MaterialSlots)
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
            "Material",
            "MaterialComponentOptions",
            true,
            canResetMaterial,
            true,
            canPasteMaterial,
            pasteUnavailableReason);

        if (header.RemoveRequested)
        {
            commands.RemoveMaterial(scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::Material;
            g_InspectorComponentClipboard.Material = *material;
            g_InspectorComponentClipboard.MaterialSlots = staticMesh->MaterialSlots;
        }

        if (header.PasteRequested && canPasteMaterial)
        {
            commands.PasteMaterialValues(
                scene,
                actor.GetID(),
                g_InspectorComponentClipboard.Material,
                g_InspectorComponentClipboard.MaterialSlots,
                projectContext);
        }

        if (header.ResetRequested)
        {
            commands.ResetMaterial(scene, actor.GetID());
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
            commands.SetMaterialBaseColor(scene, actor.GetID(), baseColor);
        }

        ImGui::Spacing();

        const bool hasTextureAssets = !projectContext.EnumerateTextureAssetIDs().empty();

        Assets::AssetPickerOptions texturePickerOptions{};
        texturePickerOptions.Type = Project::AssetType::Texture;
        texturePickerOptions.AllowNone = true;
        texturePickerOptions.TextureLibrary = &textureLibrary;

        std::string selectedTextureAssetID;

        if (DrawAssetPickerPropertyRow("Base Color Texture", projectContext, texturePickerOptions, material->BaseColorTextureAssetID, selectedTextureAssetID))
        {
            commands.SetMaterialBaseColorTextureAsset(scene, actor.GetID(), selectedTextureAssetID);
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
            const auto textureStats = textureLibrary.GetStats(textureAssetID);

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
                DrawTexturePreview(textureAssetID, textureLibrary, textureStats.value(), uiScale, 128.0f);
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
                    scene,
                    actor,
                    commands,
                    projectContext,
                    staticMesh->MaterialSlots[slotIndex],
                    static_cast<std::uint32_t>(slotIndex),
                    textureLibrary,
                    tracked,
                    hasTextureAssets,
                    openSingleSlotByDefault);
            }
        }
    }

    void DrawDirectionalLightSection(Scene::Scene& scene,
                                     Scene::Actor& actor,
                                     Editor::EditorCommands& commands,
                                     const TrackedInspectorInteraction& tracked)
    {
        Scene::DirectionalLightComponent* light = actor.TryGetComponent<Scene::DirectionalLightComponent>();
        if (!light) return;

        const bool canPasteDirectionalLight = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::DirectionalLight;

        const auto header = BeginInspectorComponent(
            "Directional Light",
            "DirectionalLightComponentOptions",
            true,
            true,
            true,
            canPasteDirectionalLight,
            canPasteDirectionalLight ? nullptr : "Copy Directional Light values first."            );

        if (header.RemoveRequested)
        {
            commands.RemoveDirectionalLight(scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::DirectionalLight;
            g_InspectorComponentClipboard.DirectionalLight = *light;
        }

        if (header.PasteRequested && canPasteDirectionalLight)
        {
            commands.PasteDirectionalLightValues(scene, actor.GetID(), g_InspectorComponentClipboard.DirectionalLight);
        }

        if (header.ResetRequested)
        {
            commands.ResetDirectionalLight(scene, actor.GetID());
        }

        if (!header.Open) return;

        bool enabled = light->Enabled;

        if (DrawCheckboxPropertyRow("Enabled", enabled))
        {
            commands.SetDirectionalLightEnabled(scene, actor.GetID(), enabled);
        }

        glm::vec3 direction = light->Direction;
        if (DrawVec3PropertyRow("Direction", direction, 0.01f, -1.0f, 1.0f, tracked))
        {
            commands.SetDirectionalLightDirection(scene, actor.GetID(), direction);
        }

        glm::vec3 color = light->Color;
        if (DrawColor3PropertyRow("Color", color, tracked))
        {
            commands.SetDirectionalLightColor(scene, actor.GetID(), color);
        }

        float intensity = light->Intensity;
        if (DrawFloatPropertyRow("Intensity", intensity, 0.01f, 0.0f, 10.0f, tracked))
        {
            commands.SetDirectionalLightIntensity(scene, actor.GetID(), intensity);
        }
    }

    void DrawRotatorSection(Scene::Scene& scene,
                            Scene::Actor& actor,
                            Editor::EditorCommands& commands,
                            const TrackedInspectorInteraction& tracked)
    {
        Scene::RotatorComponent* rotator = actor.TryGetComponent<Scene::RotatorComponent>();
        if (!rotator) return;

        const bool canPasteRotator = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::Rotator;
        const auto header = BeginInspectorComponent(
            "Rotator",
            "RotatorComponentOptions",
            true,
            true,
            true,
            canPasteRotator,
            canPasteRotator ? nullptr : "Copy Rotator values first.");

        if (header.RemoveRequested)
        {
            commands.RemoveRotator(scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::Rotator;
            g_InspectorComponentClipboard.Rotator = *rotator;
        }

        if (header.PasteRequested && canPasteRotator)
        {
            commands.PasteRotatorValues(scene, actor.GetID(), g_InspectorComponentClipboard.Rotator);
        }
        if (header.ResetRequested)
        {
            commands.ResetRotator(scene, actor.GetID());
        }
        if (!header.Open) return;

        float rotationSpeed = rotator->RotationSpeedDegreesPerSecond;

        if (DrawFloatPropertyRow("Rotation Speed", rotationSpeed, 1.0f, 0.0f, 0.0f, tracked))
        {
            commands.SetRotatorSpeed(scene, actor.GetID(), rotationSpeed);
        }
    }

    void DrawCameraSection(Scene::Scene& scene,
                           Scene::Actor& actor,
                           Editor::EditorCommands& commands,
                           Editor::EditorState& editorState,
                           const TrackedInspectorInteraction& tracked,
                           const bool isEditMode)
    {
        Scene::CameraComponent* camera = actor.TryGetComponent<Scene::CameraComponent>();
        if (!camera) return;

        const bool canPasteCamera = g_InspectorComponentClipboard.Type == InspectorComponentClipboardType::Camera;
        const auto header = BeginInspectorComponent(
            "Camera",
            "CameraComponentOptions",
            true,
            true,
            true,
            canPasteCamera,
            canPasteCamera ? nullptr : "Copy Camera values first.");

        if (header.RemoveRequested)
        {
            commands.RemoveCamera(scene, actor.GetID());
            return;
        }

        if (header.CopyRequested)
        {
            g_InspectorComponentClipboard.Type = InspectorComponentClipboardType::Camera;
            g_InspectorComponentClipboard.Camera = *camera;
        }
        if (header.PasteRequested && canPasteCamera)
        {
            commands.PasteCameraValues(scene, actor.GetID(), g_InspectorComponentClipboard.Camera);
        }
        if (header.ResetRequested)
        {
            commands.ResetCamera(scene, actor.GetID());
        }
        if (!header.Open) return;

        const bool isPilotingThisCamera =
            editorState.Selection.PilotedCameraActorID.has_value() &&
            editorState.Selection.PilotedCameraActorID.value() == actor.GetID();

        if (isEditMode)
        {
            if (!isPilotingThisCamera)
            {
                if (EditorButton("Pilot Camera"))
                {
                    editorState.Selection.PilotedCameraActorID = actor.GetID();
                }
            }
            else
            {
                if (EditorButton("Stop Piloting"))
                {
                    editorState.Selection.PilotedCameraActorID.reset();
                }
            }
        }

        bool primary = camera->Primary;
        if (DrawCheckboxPropertyRow("Primary", primary))
        {
            commands.SetCameraPrimary(scene, actor.GetID(), primary);
        }

        float fov = camera->FieldOfViewDegrees;
        if (DrawFloatPropertyRow("Field Of View", fov, 0.25f, 1.0f, 179.0f, tracked))
        {
            commands.SetCameraFieldOfView(scene, actor.GetID(), fov);
        }

        const float currentFarClip = std::isfinite(camera->FarClip) ? camera->FarClip : 1000.0f;
        const float maximumNearClip = std::clamp(currentFarClip - 0.001f, 0.001f, 100.0f);
        float nearClip = camera->NearClip;

        if (DrawFloatPropertyRow("Near Clip", nearClip, 0.01f, 0.001f, maximumNearClip, tracked))
        {
            commands.SetCameraNearClip(scene, actor.GetID(), nearClip);
        }

        const float currentNearClip = std::isfinite(camera->NearClip) ? std::clamp(camera->NearClip, 0.001f, 100.0f) : 0.1f;
        const float minimumFarClip = std::max(1.0f, currentNearClip + 0.001f);
        float farClip = camera->FarClip;

        if (DrawFloatPropertyRow("Far Clip", farClip, 1.0f, minimumFarClip, 100000.0f, tracked))
        {
            commands.SetCameraFarClip(scene, actor.GetID(), farClip);
        }
    }
}
