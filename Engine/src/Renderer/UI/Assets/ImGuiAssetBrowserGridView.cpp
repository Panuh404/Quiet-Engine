#include "Renderer/UI/Assets/ImGuiAssetBrowserGridView.h"

#include <utility>
#include <vector>
#include <algorithm>
#include <string>

#include <imgui.h>

#include "Quiet/Platform/Shell.h"
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

    void DrawAssetMeshGlyph(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const bool compact)
    {
        const ImVec2 center(min.x + (max.x - min.x) * 0.5f, min.y + (max.y - min.y) * 0.48f);
        const float radius = std::min(max.x - min.x, max.y - min.y) * (compact ? 0.31f : 0.26f);

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

        if (compact) return;
        const char* label = "MESH";

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddText(
            ImVec2(min.x + (max.x - min.x - textSize.x) * 0.5f, max.y - textSize.y - SZ::Px(10.0f)), muted,
            label
        );
    }

    void DrawAssetBrowserCardPreview(
        const Quiet::Project::AssetRecord& record,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
        const Quiet::Renderer::UI::Assets::AssetBrowserCardMetrics& metrics,
        const ImVec2& min,
        const ImVec2& max)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
            metrics.PreviewRounding
        );

        drawList->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            metrics.PreviewRounding
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
                    const float availableWidth = max.x - min.x - metrics.PreviewInnerPadding * 2.0f;
                    const float availableHeight = max.y - min.y - metrics.PreviewInnerPadding * 2.0f;

                    const float maximumSide = std::max(SZ::Px(20.0f), std::min(availableWidth, availableHeight));
                    const ImVec2 previewSize = Quiet::Renderer::UI::CalculateTexturePreviewSize(textureStats->Width, textureStats->Height, maximumSide);

                    const ImVec2 imageMin(
                        min.x + (max.x - min.x - previewSize.x) * 0.5f,
                        min.y + (max.y - min.y - previewSize.y) * 0.5f);

                    const ImVec2 imageMax(
                        imageMin.x + previewSize.x,
                        imageMin.y + previewSize.y);

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
                        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)),
                        metrics.PreviewRounding
                    );

                    return;
                }
            }
        }

        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            DrawAssetMeshGlyph(drawList, min, max, metrics.CompactPreview);
            return;
        }

        const char* fallbackLabel = nullptr;

        switch (record.Type)
        {
            case Quiet::Project::AssetType::Texture:
                fallbackLabel = metrics.CompactPreview ? "TEX" : "TEXTURE";
                break;

            case Quiet::Project::AssetType::Audio:
                fallbackLabel = metrics.CompactPreview ? "AUD" : "AUDIO";
                break;

            case Quiet::Project::AssetType::Mesh:
                fallbackLabel = metrics.CompactPreview ? "MESH" : "MESH";
                break;

            case Quiet::Project::AssetType::Script:
                fallbackLabel = metrics.CompactPreview ? "LUA" : "SCRIPT";
                break;

            case Quiet::Project::AssetType::Unknown:
            default:
                fallbackLabel = metrics.CompactPreview ? "?" : "ASSET";
                break;
        }

        const ImVec2 textSize = ImGui::CalcTextSize(fallbackLabel);
        drawList->AddText(
            ImVec2(min.x + (max.x - min.x - textSize.x) * 0.5f, min.y + (max.y - min.y - textSize.y) * 0.5f),
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
            fallbackLabel
        );
    }

    void DrawAssetFolderGlyph(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const float width = max.x - min.x;
        const float height = max.y - min.y;

        const float maximumWidthFromHeight = height * 0.80f / 0.64f;
        const float folderWidth = std::min(width * 0.62f, maximumWidthFromHeight);
        const float folderHeight = folderWidth * 0.64f;

        const ImVec2 folderMin(min.x + (width - folderWidth) * 0.5f, min.y + (height - folderHeight) * 0.5f);
        const ImVec2 folderMax(folderMin.x + folderWidth, folderMin.y + folderHeight);

        const float tabWidth = folderWidth * 0.42f;
        const float tabHeight = folderHeight * 0.22f;

        const ImU32 folderColor = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Warning);
        const ImU32 folderShadow = ImGui::ColorConvertFloat4ToU32(ImVec4(0.160f, 0.110f, 0.025f, 1.0f));

        drawList->AddRectFilled(
            ImVec2(folderMin.x, folderMin.y + tabHeight),
            folderMax,
            folderShadow,
            SZ::Px(7.0f)
        );

        drawList->AddRectFilled(
            folderMin,
            ImVec2(
                folderMin.x + tabWidth,
                folderMin.y + tabHeight * 1.5f
            ),
            folderColor,
            SZ::Px(5.0f)
        );

        drawList->AddRectFilled(
            ImVec2(folderMin.x, folderMin.y + tabHeight),
            folderMax,
            folderColor,
            SZ::Px(7.0f)
        );
    }

    void DrawAssetCardSelectionIndicator(ImDrawList* drawList, const ImVec2& cardMin, const ImVec2& cardMax, const bool selected)
    {
        if (!selected) return;

        const float radius = SZ::Px(8.0f);
        const ImVec2 center(cardMax.x - SZ::Px(14.0f), cardMin.y + SZ::Px(14.0f));

        drawList->AddCircleFilled( center, radius, ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Accent));

        const ImU32 checkColor = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Text);
        drawList->AddLine(
            ImVec2(center.x - SZ::Px(3.5f), center.y),
            ImVec2(center.x - SZ::Px(1.0f), center.y + SZ::Px(3.0f)),
            checkColor,
            SZ::Px(1.8f)
        );

        drawList->AddLine(
            ImVec2(center.x - SZ::Px(1.0f), center.y + SZ::Px(3.0f)),
            ImVec2(center.x + SZ::Px(4.0f), center.y - SZ::Px(3.5f)),
            checkColor,
            SZ::Px(1.8f)
        );
    }

    void DrawAssetCardIssueStrip(
        ImDrawList* drawList,
        const ImVec2& cardMin,
        const ImVec2& cardMax,
        const Quiet::Renderer::UI::Assets::AssetBrowserCardMetrics& metrics,
        const Quiet::Renderer::UI::Assets::AssetPathDiagnostics& diagnostics)
    {
        if (!diagnostics.HasIssue()) return;

        const ImVec4 issueColor = diagnostics.IsMissing() ? QUI::EditorColor::Error : QUI::EditorColor::Warning;
        const float verticalInset = std::max(SZ::Px(5.0f), metrics.CardRounding * 0.55f);

        drawList->AddRectFilled(
            ImVec2(cardMin.x + SZ::Px(1.0f), cardMin.y + verticalInset),
            ImVec2(cardMin.x + SZ::Px(4.0f), cardMax.y - verticalInset),
            ImGui::ColorConvertFloat4ToU32(issueColor),
            SZ::Px(2.0f)
        );
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserGrid(
        const std::vector<AssetBrowserFolderEntry>& visibleFolders,
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::TextureLibrary& textureLibrary,
        AssetBrowserState& state,
        std::string& pendingNavigateFolder,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        AssetBrowserOperationRequest& operationRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        AssetBrowserThumbnailSize thumbnailSize)
    {
        auto& selectedContentFolder = state.SelectedContentFolder;
        const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;
        const AssetBrowserCardMetrics metrics = MakeAssetBrowserCardMetrics(thumbnailSize);
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float columnStride = metrics.TargetCardWidth + metrics.GridSpacing;
        const int columnCount = std::max(1, static_cast<int>((availableWidth + metrics.GridSpacing) / std::max(SZ::Px(1.0f), columnStride)));

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(metrics.GridSpacing * 0.5f, metrics.GridSpacing * 0.5f));

        if (!ImGui::BeginTable("AssetBrowserGrid", columnCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
        {
            ImGui::PopStyleVar();
            return;
        }

        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
        }

        for (const AssetBrowserFolderEntry& folder : visibleFolders)
        {
            ImGui::TableNextColumn();

            const bool isSelectedFolder = selectedContentFolder == folder.RelativePath;

            const float cardWidth = std::max(SZ::Px(1.0f), ImGui::GetContentRegionAvail().x);
            const float cardHeight = metrics.CardHeight;
            const float padding = metrics.Padding;
            const float previewHeight = metrics.PreviewHeight;

            const ImVec2 cardMin = ImGui::GetCursorScreenPos();
            const ImVec2 cardSize(cardWidth, cardHeight);
            const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);
            const std::string hitID = "##AssetBrowserFolderCard_" + folder.RelativePath;

            ImGui::InvisibleButton(hitID.c_str(), cardSize);
            const bool hovered = ImGui::IsItemHovered();

            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                ClearAssetSelection(state);
                selectedContentFolder = folder.RelativePath;
            }

            if (IsAssetBrowserContextMenuRequestedForLastItem())
            {
                ClearAssetSelection(state);
                selectedContentFolder = folder.RelativePath;
            }

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                pendingNavigateFolder = folder.RelativePath;
            }

            std::vector<std::string> droppedAssetIDs;
            if (AcceptAssetBrowserDrop(droppedAssetIDs))
            {
                assetMoveRequest.AssetIDs = std::move(droppedAssetIDs);
                assetMoveRequest.TargetFolder = folder.RelativePath;
            }

            DrawAssetFolderContextMenu(
                hitID.c_str(),
                projectContext,
                folder.RelativePath,
                runtimeModeActive,
                state,
                operationRequest,
                folderActionRequest
            );
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const ImVec4 cardColor = isSelectedFolder
                ? EditorColor::AccentDim
                : hovered
                    ? EditorColor::Background3
                    : EditorColor::Background2;

            const ImVec4 borderColor = isSelectedFolder
                ? EditorColor::AccentSoft
                : hovered
                    ? EditorColor::Border
                    : EditorColor::BorderSoft;

            drawList->AddRectFilled(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(cardColor),
                metrics.CardRounding
            );

            drawList->AddRect(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(borderColor),
                metrics.CardRounding,
                0,
                isSelectedFolder ? SZ::Px(2.0f) : SZ::Px(1.0f)
            );

            const ImVec2 previewMin(cardMin.x + padding, cardMin.y + padding);
            const ImVec2 previewMax(cardMax.x - padding, previewMin.y + previewHeight);
            const ImVec2 afterCardCursor = ImGui::GetCursorScreenPos();

            drawList->AddRectFilled(
                previewMin,
                previewMax,
                ImGui::ColorConvertFloat4ToU32(
                    EditorColor::Background3
                ),
                metrics.PreviewRounding
            );

            drawList->AddRect(
                previewMin,
                previewMax,
                ImGui::ColorConvertFloat4ToU32(
                    EditorColor::BorderSoft
                ),
                metrics.PreviewRounding
            );

            DrawAssetFolderGlyph(drawList, previewMin, previewMax);
            float contentY = previewMax.y + metrics.ContentGap;

            const ImVec2 nameMin(cardMin.x + padding, contentY);
            const ImVec2 nameMax(cardMax.x - padding, nameMin.y + ImGui::GetTextLineHeight());
            DrawAssetCardTextLine(folder.Name, nameMin, nameMax, isSelectedFolder ? EditorColor::Accent : EditorColor::Text);

            contentY = nameMax.y;
            if (metrics.ShowSecondaryLine)
            {
                contentY += metrics.SecondaryGap;
                const ImVec2 secondaryMin(cardMin.x + padding, contentY);
                const ImVec2 secondaryMax(cardMax.x - padding, secondaryMin.y + ImGui::GetTextLineHeight());

                DrawAssetCardTextLine(folder.RelativePath, secondaryMin, secondaryMax, EditorColor::TextDim);
                contentY = secondaryMax.y;
            }

            if (metrics.ShowMetadataPill)
            {
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + padding, contentY + metrics.MetadataGap));
                DrawAssetFolderPill();
            }

            DrawAssetCardSelectionIndicator(drawList, cardMin, cardMax, isSelectedFolder);
            if (hovered)
            {
                DrawAssetPaddedTooltip(folder.RelativePath + "\n\nDouble-click to open.");
            }

            ImGui::SetCursorScreenPos(afterCardCursor);
        }

        for (const Project::AssetRecord* recordPtr : visibleAssets)
        {
            if (!recordPtr) continue;

            const Project::AssetRecord& record = *recordPtr;
            ImGui::TableNextColumn();

            const std::string displayName = MakeAssetDisplayName(record);
            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
            const bool isSelectedAsset = IsAssetSelected(state, record.ID);

            const float cardWidth = std::max(SZ::Px(1.0f), ImGui::GetContentRegionAvail().x);
            const float cardHeight = metrics.CardHeight;
            const float padding = metrics.Padding;
            const float previewHeight = metrics.PreviewHeight;

            const ImVec2 cardMin = ImGui::GetCursorScreenPos();
            const ImVec2 cardSize(cardWidth, cardHeight);
            const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);

            const std::string hitID = "##AssetBrowserCard_" + record.ID;
            ImGui::InvisibleButton(hitID.c_str(), cardSize);
            const bool hovered = ImGui::IsItemHovered();

            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                const ImGuiIO& io = ImGui::GetIO();
                HandleAssetSelectionClick(state, visibleAssets, record.ID, io.KeyCtrl, io.KeyShift);
            }

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (record.Type == Project::AssetType::Script && diagnostics.Exists && diagnostics.RegularFile)
                {
                    Platform::OpenPathWithDefaultApplication(diagnostics.ResolvedPath);
                }
                else
                {
                    pendingDetailsAssetID = record.ID;
                }
            }

            if (IsAssetBrowserContextMenuRequestedForLastItem())
            {
                if (!IsAssetSelected(state, record.ID))
                {
                    SelectSingleAsset(state, record.ID);
                }
                else
                {
                    state.SelectedDetailsAssetID = record.ID;
                    state.SelectedContentFolder.clear();
                }
            }

            DrawAssetContextMenu(
                hitID.c_str(),
                projectContext,
                record,
                runtimeModeActive,
                state,
                operationRequest,
                pendingDetailsAssetID,
                pendingReimportAssetID
            );

            DrawAssetBrowserDragSource(record, state, !runtimeModeActive);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const ImVec4 cardColor = isSelectedAsset
                ? EditorColor::AccentDim
                : hovered
                    ? EditorColor::Background3
                    : EditorColor::Background2;

            const ImVec4 cardBorderColor = isSelectedAsset
                ? EditorColor::AccentSoft : diagnostics.IsMissing()
                    ? EditorColor::Error : diagnostics.External
                        ? EditorColor::Warning : hovered
                            ? EditorColor::Border : EditorColor::BorderSoft;

            drawList->AddRectFilled(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(cardColor),
                metrics.CardRounding
            );

            drawList->AddRect(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(cardBorderColor),
                metrics.CardRounding,
                0,
                isSelectedAsset ? SZ::Px(2.0f) : SZ::Px(1.0f)
            );

            DrawAssetCardIssueStrip(drawList, cardMin, cardMax, metrics, diagnostics);

            const ImVec2 previewMin(
                cardMin.x + padding,
                cardMin.y + padding
            );

            const ImVec2 previewMax(
                cardMax.x - padding,
                previewMin.y + previewHeight
            );

            const ImVec2 afterCardCursor =
                ImGui::GetCursorScreenPos();

            DrawAssetBrowserCardPreview(record, textureLibrary, metrics, previewMin, previewMax);

            if (diagnostics.HasIssue() && metrics.ShowDiagnosticPill)
            {
                ImGui::SetCursorScreenPos(ImVec2(previewMin.x + SZ::Px(6.0f), previewMin.y + SZ::Px(6.0f)));
                DrawAssetDiagnosticPill(MakeAssetPathDiagnosticLabel(diagnostics), diagnostics.IsMissing());
            }

            float contentY = previewMax.y + metrics.ContentGap;
            const ImVec2 nameMin(cardMin.x + padding, contentY);
            const ImVec2 nameMax(cardMax.x - padding, nameMin.y + ImGui::GetTextLineHeight());

            DrawAssetCardTextLine(displayName, nameMin, nameMax, isSelectedAsset ? EditorColor::Accent : EditorColor::Text);

            contentY = nameMax.y;
            if (metrics.ShowSecondaryLine)
            {
                contentY += metrics.SecondaryGap;
                const ImVec2 secondaryMin(cardMin.x + padding, contentY);
                const ImVec2 secondaryMax(cardMax.x - padding, secondaryMin.y + ImGui::GetTextLineHeight());

                DrawAssetCardTextLine(record.ID, secondaryMin, secondaryMax, EditorColor::TextDim);
                contentY = secondaryMax.y;
            }

            if (metrics.ShowMetadataPill)
            {
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + padding, contentY + metrics.MetadataGap));
                DrawAssetTypePill(record.Type);
            }

            DrawAssetCardSelectionIndicator(drawList, cardMin, cardMax, isSelectedAsset);

            if (hovered)
            {
                if (diagnostics.HasIssue())
                {
                    std::string tooltip = MakeAssetPathDiagnosticDescription(diagnostics);
                    if (!record.Path.empty())
                    {
                        tooltip += "\n\nRegistered path: " + record.Path;
                    }
                    DrawAssetPaddedTooltip(tooltip);
                }
                else
                {
                    DrawAssetPaddedTooltip(!record.Path.empty() ? record.Path : record.ID);
                }
            }
            ImGui::SetCursorScreenPos(afterCardCursor);
        }

        ImGui::EndTable();
        ImGui::PopStyleVar();

        DrawAssetBrowserBackgroundContextMenu(
            "##AssetBrowserGridBackgroundContext",
            projectContext,
            state.CurrentAssetFolder,
            runtimeModeActive,
            state,
            operationRequest,
            folderActionRequest
        );

        const bool emptyContentClicked =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered();

        if (emptyContentClicked)
        {
            ClearAssetSelection(state);
            selectedContentFolder.clear();
        }
    }
}