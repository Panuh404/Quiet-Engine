#include "Renderer/UI/Assets/ImGuiAssetBrowserGridView.h"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    void DrawAssetPreviewCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const float cellSize = SZ::Px(8.0f);
        const ImU32 colorA = ImGui::ColorConvertFloat4ToU32(ImVec4(0.105f, 0.115f, 0.135f, 1.0f));
        const ImU32 colorB = ImGui::ColorConvertFloat4ToU32(ImVec4(0.075f, 0.085f, 0.105f, 1.0f));

        int row = 0;
        for (float y = min.y; y < max.y; y += cellSize, ++row)
        {
            int column = 0;
            for (float x = min.x; x < max.x; x += cellSize, ++column)
            {
                const ImVec2 cellMin(x, y);
                const ImVec2 cellMax(
                    std::min(x + cellSize, max.x),
                    std::min(y + cellSize, max.y)
                );
                drawList->AddRectFilled(cellMin, cellMax, ((row + column) % 2 == 0) ? colorA : colorB);
            }
        }
    }

    void DrawAssetMeshGlyph(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const ImVec2 center(
            min.x + (max.x - min.x) * 0.5f,
            min.y + (max.y - min.y) * 0.48f
        );

        const float radius = std::min(max.x - min.x, max.y - min.y) * 0.26f;
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentSoft);
        const ImU32 muted = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted);

        const ImVec2 top(center.x, center.y - radius);
        const ImVec2 left(center.x - radius * 0.92f, center.y + radius * 0.54f);
        const ImVec2 right(center.x + radius * 0.92f, center.y + radius * 0.54f);
        const ImVec2 inner(center.x, center.y + radius * 0.06f);

        drawList->AddTriangle(top, right, left, accent, SZ::Px(2.0f));
        drawList->AddLine(top, inner, muted, SZ::Px(1.5f));
        drawList->AddLine(left, inner, muted, SZ::Px(1.5f));
        drawList->AddLine(right, inner, muted, SZ::Px(1.5f));

        const char* label = "MESH";
        const ImVec2 textSize = ImGui::CalcTextSize(label);

        drawList->AddText(
            ImVec2(
                min.x + (max.x - min.x - textSize.x) * 0.5f,
                max.y - textSize.y - SZ::Px(10.0f)
            ),
            muted,
            label
        );
    }

    void DrawAssetBrowserCardPreview(
        const Quiet::Project::AssetRecord& record,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const ImVec2& min,
        const ImVec2& max,
        const float uiScale)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
            SZ::Px(8.0f)
        );

        drawList->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            SZ::Px(8.0f)
        );

        if (record.Type == Quiet::Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value() && textureStats->GpuReady)
            {
                DrawAssetPreviewCheckerboard(drawList, min, max);
                const ImTextureID previewID = Quiet::Renderer::UI::GetOrCreateTexturePreviewID(record.ID, textureLibrary);

                if (previewID)
                {
                    const float availableWidth = max.x - min.x - SZ::Px(16.0f);
                    const float availableHeight = max.y - min.y - SZ::Px(16.0f);

                    const float maxSide =
                        std::max(
                            SZ::Px(32.0f),
                            std::min(availableWidth, availableHeight)
                        );

                    const ImVec2 previewSize =
                        Quiet::Renderer::UI::CalculateTexturePreviewSize(
                            textureStats->Width,
                            textureStats->Height,
                            maxSide
                        );

                    const ImVec2 imageMin(
                        min.x + (max.x - min.x - previewSize.x) * 0.5f,
                        min.y + (max.y - min.y - previewSize.y) * 0.5f
                    );

                    const ImVec2 imageMax(
                        imageMin.x + previewSize.x,
                        imageMin.y + previewSize.y
                    );

                    drawList->AddImage(
                        previewID,
                        imageMin,
                        imageMax,
                        ImVec2(0.0f, 1.0f),
                        ImVec2(1.0f, 0.0f),
                        IM_COL32_WHITE
                    );

                    drawList->AddRect(
                        imageMin,
                        imageMax,
                        ImGui::ColorConvertFloat4ToU32(
                            ImVec4(1.0f, 1.0f, 1.0f, 0.10f)
                        ),
                        SZ::Px(3.0f)
                    );

                    return;
                }
            }
        }

        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            DrawAssetMeshGlyph(drawList, min, max);
        }
        else
        {
            const char* fallbackLabel = record.Type == Quiet::Project::AssetType::Texture ? "TEXTURE" : "ASSET";
            const ImVec2 textSize = ImGui::CalcTextSize(fallbackLabel);

            drawList->AddText(
                ImVec2(
                    min.x + (max.x - min.x - textSize.x) * 0.5f,
                    min.y + (max.y - min.y - textSize.y) * 0.5f
                ),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                fallbackLabel
            );
        }

        (void)uiScale;
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserGrid(
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::TextureLibrary& textureLibrary,
        std::string& selectedDetailsAssetID,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID,
        const AssetBrowserThumbnailSize thumbnailSize,
        const float uiScale)
    {
        const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;
        const AssetBrowserCardMetrics metrics = MakeAssetBrowserCardMetrics(thumbnailSize);
        const float availableWidth = ImGui::GetContentRegionAvail().x;

        const int columnCount =
            std::max(
                1,
                static_cast<int>(
                    availableWidth /
                    std::max(SZ::Px(1.0f), metrics.DesiredColumnWidth)
                )
            );

        if (!ImGui::BeginTable("AssetBrowserGrid", columnCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
            return;

        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
        }

        for (const Project::AssetRecord* recordPtr : visibleAssets)
        {
            if (!recordPtr) continue;

            const Project::AssetRecord& record = *recordPtr;
            ImGui::TableNextColumn();

            const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
            const std::string displayName = MakeAssetDisplayName(record);
            const bool isSelectedAsset = selectedDetailsAssetID == record.ID;

            const float cardWidth =
                std::max(
                    metrics.MinimumCardWidth,
                    ImGui::GetContentRegionAvail().x
                );

            const float cardHeight = metrics.CardHeight;
            const float padding = metrics.Padding;
            const float previewHeight = metrics.PreviewHeight;

            const ImVec2 cardMin = ImGui::GetCursorScreenPos();
            const ImVec2 cardSize(cardWidth, cardHeight);
            const ImVec2 cardMax(
                cardMin.x + cardSize.x,
                cardMin.y + cardSize.y
            );

            const std::string hitID = "##AssetBrowserCard_" + record.ID;
            ImGui::InvisibleButton(hitID.c_str(), cardSize);
            const bool hovered = ImGui::IsItemHovered();

            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                selectedDetailsAssetID = record.ID;

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                pendingDetailsAssetID = record.ID;

            DrawAssetContextMenu(
                hitID.c_str(),
                projectContext,
                record,
                runtimeModeActive,
                referenceCount,
                selectedDetailsAssetID,
                pendingDetailsAssetID,
                pendingReimportAssetID,
                pendingRemoveAssetID
            );

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const ImVec4 cardColor =
                isSelectedAsset
                    ? EditorColor::AccentDim
                    : hovered
                        ? EditorColor::Background3
                        : EditorColor::Background2;

            drawList->AddRectFilled(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(cardColor),
                SZ::Px(12.0f)
            );

            drawList->AddRect(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(
                    isSelectedAsset
                        ? EditorColor::AccentSoft
                        : EditorColor::BorderSoft
                ),
                SZ::Px(12.0f)
            );

            const ImVec2 previewMin(
                cardMin.x + padding,
                cardMin.y + padding
            );

            const ImVec2 previewMax(
                cardMax.x - padding,
                previewMin.y + previewHeight
            );

            DrawAssetBrowserCardPreview(
                record,
                textureLibrary,
                previewMin,
                previewMax,
                uiScale
            );

            const ImVec2 afterCardCursor = ImGui::GetCursorScreenPos();

            const ImVec2 nameMin(
                cardMin.x + padding,
                previewMax.y + SZ::Px(8.0f)
            );

            const ImVec2 nameMax(
                cardMax.x - padding,
                nameMin.y + ImGui::GetTextLineHeight()
            );

            DrawAssetCardTextLine(
                displayName,
                nameMin,
                nameMax,
                isSelectedAsset
                    ? EditorColor::Accent
                    : EditorColor::Text
            );

            const ImVec2 typePillPos(
                cardMin.x + padding,
                nameMax.y + SZ::Px(8.0f)
            );

            ImGui::SetCursorScreenPos(typePillPos);
            DrawAssetTypePill(record.Type);

            if (hovered)
            {
                DrawAssetPaddedTooltip(!record.Path.empty() ? record.Path : record.ID);
            }

            ImGui::SetCursorScreenPos(afterCardCursor);
        }

        ImGui::EndTable();
    }
}