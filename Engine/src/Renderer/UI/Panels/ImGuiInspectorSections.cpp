#include "ImGuiInspectorSections.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>

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

    struct InspectorComponentHeaderResult
    {
        bool Open = false;
        bool RemoveRequested = false;
        bool ResetRequested = false;
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

    void DrawFloatingComponentOptionsMenu(const char* popupID, const bool removable, const bool resettable, InspectorComponentHeaderResult& result)
    {
        if (g_OpenInspectorComponentMenuID != popupID) return;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            g_OpenInspectorComponentMenuID.clear();
            return;
        }

        const float menuWidth = SZ::Px(230.0f);
        const float estimatedMenuHeight = SZ::Px(48.0f);

        const ImVec2 menuSize(menuWidth, estimatedMenuHeight);
        const ImVec2 menuPosition = ClampMenuPositionToViewport(g_OpenInspectorComponentMenuPosition, menuSize);

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

            if (resettable)
            {
                if (QUI::EditorSelectable("Reset Component", false, 0, itemSize))
                {
                    result.ResetRequested = true;
                    g_OpenInspectorComponentMenuID.clear();
                }
                ImGui::Separator();
            }

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

    InspectorComponentHeaderResult BeginInspectorComponent(const char* title, const char* popupID, const bool removable = true, const bool resettable = false)
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

    void DrawMaterialTexturePreviewIfAvailable(
        const std::string& textureAssetID,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const float uiScale,
        const float previewSize)
    {
        if (textureAssetID.empty()) return;
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

        QUI::DrawTexturePreview(textureAssetID, textureLibrary, textureStats.value(), uiScale, previewSize);
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

    bool DrawFloatPropertyRow(const char* label, float& value, const float speed, const float minValue, const float maxValue,
                              const QUI::TrackedInspectorInteraction& tracked)
    {
        ImGui::PushID(label);
        BeginInspectorPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::DragFloat("##Value", &value, speed, minValue, maxValue, "%.3f");

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool DrawVec3PropertyRow(const char* label, glm::vec3& value, const float speed, const float minValue, const float maxValue,
                             const QUI::TrackedInspectorInteraction& tracked)
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
            changed |= ImGui::DragFloat("##Axis", &axisValue, speed, minValue, maxValue, "%.3f");

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
        if (!actor.StaticMesh.has_value()) return {};
        if (!actor.StaticMesh->Mesh) return {};

        const auto& importedSlots = actor.StaticMesh->Mesh->GetImportedMaterialSlots();
        if (slotIndex >= importedSlots.size()) return {};
        return importedSlots[slotIndex].Name;
    }

    std::size_t CountSubmeshesUsingMaterialSlot(const Quiet::Scene::Actor& actor, const std::uint32_t slotIndex)
    {
        if (!actor.StaticMesh.has_value()) return 0;
        if (!actor.StaticMesh->Mesh) return 0;

        const auto& submeshes = actor.StaticMesh->Mesh->GetSubmeshes();
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
                commands.SetStaticMeshMaterialSlotBaseColor(scene, actor.ID, slotIndex, baseColor);
            }

            ASSETS::AssetPickerOptions texturePickerOptions{};
            texturePickerOptions.Type = Quiet::Project::AssetType::Texture;
            texturePickerOptions.AllowNone = true;
            texturePickerOptions.TextureLibrary = &textureLibrary;

            std::string selectedTextureAssetID;
            if (DrawAssetPickerPropertyRow("Base Color Texture", projectContext, texturePickerOptions, materialSlot.BaseColorTextureAssetID, selectedTextureAssetID))
            {
                commands.SetStaticMeshMaterialSlotBaseColorTextureAsset(scene, actor.ID, slotIndex, selectedTextureAssetID);
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

    void DrawActorDetailsSection(Scene::Scene& scene,
                                 Scene::Actor& actor,
                                 Editor::EditorCommands& commands,
                                 const TrackedInspectorInteraction& tracked)
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

        const auto transformHeader = BeginInspectorComponent(
            "Transform",
            "TransformComponentOptions",
            false,
            true
        );

        if (transformHeader.ResetRequested)
        {
            commands.ResetActorTransform(scene, actor.ID);
        }

        if (transformHeader.Open)
        {
            auto& transform = actor.Transform.WorldTransform;

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
        const bool hasMeshAssets = !projectContext.EnumerateMeshAssetIDs().empty();
        if (!actor.StaticMesh.has_value()) return;

        const auto header = BeginInspectorComponent("Static Mesh", "StaticMeshComponentOptions");
        if (header.RemoveRequested)
        {
            commands.RemoveStaticMesh(scene, actor.ID);
            return;
        }
        if (!header.Open) return;

        Assets::AssetPickerOptions meshPickerOptions{};
        meshPickerOptions.Type = Project::AssetType::Mesh;
        meshPickerOptions.MeshLibrary = &meshLibrary;
        std::string selectedMeshAssetID;

        if (DrawAssetPickerPropertyRow("Mesh Resource", projectContext, meshPickerOptions, actor.StaticMesh->MeshResourceID, selectedMeshAssetID))
        {
            commands.SetStaticMeshResource(scene, actor.ID, selectedMeshAssetID, projectContext, meshLibrary);
        }
        if (!hasMeshAssets)
        {
            DrawInspectorHint("No mesh assets registered.");
        }

        bool visible = actor.StaticMesh->Visible;
        if (DrawCheckboxPropertyRow("Visible", visible))
        {
            commands.SetStaticMeshVisible(scene, actor.ID, visible);
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
        if (!actor.StaticMesh.has_value()) return;
        if (!actor.Material.has_value()) return;

        const bool canResetMaterial = actor.StaticMesh->Mesh != nullptr;

        const auto header = BeginInspectorComponent("Material", "MaterialComponentOptions", true, canResetMaterial);
        if (header.RemoveRequested)
        {
            commands.RemoveMaterial(scene, actor.ID);
            return;
        }
        if (header.ResetRequested)
        {
            commands.ResetMaterial(scene, actor.ID);
        }
        if (!header.Open) return;

        const bool hasMaterialSlots = !actor.StaticMesh->MaterialSlots.empty();
        if (hasMaterialSlots)
        {
            DrawInspectorSubsectionHeader("Fallback Material");
            DrawInspectorHint("Used when a submesh has no matching imported material slot.");

            ImGui::Spacing();
        }

        glm::vec4 baseColor = actor.Material->BaseColor;
        if (DrawColor4PropertyRow("Base Color", baseColor, tracked))
            commands.SetMaterialBaseColor(scene, actor.ID, baseColor);

        ImGui::Spacing();

        const bool hasTextureAssets = !projectContext
                .EnumerateTextureAssetIDs()
                .empty();

        Assets::AssetPickerOptions texturePickerOptions{};
        texturePickerOptions.Type = Project::AssetType::Texture;
        texturePickerOptions.AllowNone = true;
        texturePickerOptions.TextureLibrary = &textureLibrary;

        std::string selectedTextureAssetID;

        if (DrawAssetPickerPropertyRow("Base Color Texture", projectContext, texturePickerOptions,
            actor.Material->BaseColorTextureAssetID, selectedTextureAssetID))
        {
            commands.SetMaterialBaseColorTextureAsset(scene, actor.ID, selectedTextureAssetID);
        }

        if (!hasTextureAssets)
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
                DrawTexturePreview(textureAssetID, textureLibrary, textureStats.value(), uiScale, 128.0f);
            }
        }

        if (hasMaterialSlots)
        {
            const std::size_t slotCount = actor.StaticMesh->MaterialSlots.size();
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
                    actor.StaticMesh->MaterialSlots[slotIndex],
                    static_cast<std::uint32_t>(slotIndex),
                    textureLibrary,
                    tracked,
                    hasTextureAssets,
                    openSingleSlotByDefault
                );
            }
        }
    }

    void DrawDirectionalLightSection(Scene::Scene& scene, Scene::Actor& actor, Editor::EditorCommands& commands, const TrackedInspectorInteraction& tracked)
    {
        if (!actor.DirectionalLight.has_value()) return;

        const auto header = BeginInspectorComponent("Directional Light", "DirectionalLightComponentOptions", true, true);
        if (header.RemoveRequested)
        {
            commands.RemoveDirectionalLight(scene, actor.ID);
            return;
        }

        if (header.ResetRequested)
        {
            commands.ResetDirectionalLight(scene, actor.ID);
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

    void DrawRotatorSection(Scene::Scene& scene, Scene::Actor& actor, Editor::EditorCommands& commands, const TrackedInspectorInteraction& tracked)
    {
        if (!actor.Rotator.has_value()) return;

        const auto header = BeginInspectorComponent("Rotator", "RotatorComponentOptions", true, true);

        if (header.RemoveRequested)
        {
            commands.RemoveRotator(scene, actor.ID);
            return;
        }

        if (header.ResetRequested)
        {
            commands.ResetRotator(scene, actor.ID);
        }

        if (!header.Open)
            return;

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

        const auto header = BeginInspectorComponent("Camera", "CameraComponentOptions", true, true);
        if (header.RemoveRequested)
        {
            commands.RemoveCamera(scene, actor.ID);
            return;
        }
        if (header.ResetRequested)
        {
            commands.ResetCamera(scene, actor.ID);
        }

        if (!header.Open)
            return;

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
