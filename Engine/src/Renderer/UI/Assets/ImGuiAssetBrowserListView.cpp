#include "Renderer/UI/Assets/ImGuiAssetBrowserListView.h"

#include <utility>
#include <vector>
#include <string>
#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Audio/AudioLibrary.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserList(
        const std::vector<AssetBrowserFolderEntry>& visibleFolders,
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        const Audio::AudioLibrary& audioLibrary,
        AssetBrowserState& state,
        std::string& pendingNavigateFolder,
        AssetBrowserFolderActionRequest& folderActionRequest,
        AssetBrowserAssetMoveRequest& assetMoveRequest,
        AssetBrowserOperationRequest& operationRequest,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID)
    {
        auto& selectedContentFolder = state.SelectedContentFolder;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, QUI::EditorColor::Border);
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, QUI::EditorColor::BorderSoft);
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.030f, 0.036f, 0.048f, 0.45f));

        if (ImGui::BeginTable("AssetsTable", 4,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, SZ::Px(92.0f));
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, SZ::Px(190.0f));
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, SZ::Px(96.0f));
            ImGui::TableHeadersRow();

            const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;

            for (const AssetBrowserFolderEntry& folder : visibleFolders)
            {
                const bool isSelectedFolder = selectedContentFolder == folder.RelativePath;
                const float rowHeight = SZ::Px(42.0f);
                const float contentOffsetY = SZ::Px(9.0f);

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                if (isSelectedFolder)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(EditorColor::AccentDim));
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::ColorConvertFloat4ToU32(EditorColor::AccentDim));
                }
                bool rowHovered = false;

                // Folder
                ImGui::TableSetColumnIndex(0);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetFolderRow_" + folder.RelativePath;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                    {
                        ClearAssetSelection(state);
                        selectedContentFolder = folder.RelativePath;
                    }

                    const bool cellHovered = ImGui::IsItemHovered();
                    rowHovered |= cellHovered;

                    if (IsAssetBrowserContextMenuRequestedForLastItem())
                    {
                        ClearAssetSelection(state);
                        selectedContentFolder = folder.RelativePath;
                    }

                    if (cellHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
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
                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + contentOffsetY));
                    ImGui::PushStyleColor(ImGuiCol_Text, isSelectedFolder ? EditorColor::Accent : EditorColor::Text);
                    ImGui::TextUnformatted(folder.Name.c_str());
                    ImGui::PopStyleColor();

                    if (cellHovered)
                    {
                        DrawAssetPaddedTooltip(folder.RelativePath + "\n\nDouble-click to open.");
                    }
                }

                // Type
                ImGui::TableSetColumnIndex(1);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                    DrawAssetFolderPill();
                }

                // State
                ImGui::TableSetColumnIndex(2);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                    DrawAssetStatePill("Physical Folder");
                }

                // Actions
                ImGui::TableSetColumnIndex(3);
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SZ::Px(4.0f));
                    const std::string openLabel = "Open##" + folder.RelativePath;
                    if (EditorButton(openLabel.c_str(), SZ::Px(ImVec2(82.0f, 0.0f))))
                    {
                        pendingNavigateFolder = folder.RelativePath;
                    }
                }

                if (rowHovered && !isSelectedFolder)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(EditorColor::Background3));
                }
            }

            for (const Project::AssetRecord* recordPtr : visibleAssets)
            {
                if (!recordPtr) continue;
                const Project::AssetRecord& record = *recordPtr;
                const std::uint32_t referenceCount = CountAssetReferences(scene, record);
                const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
                const std::string status = MakeStatusLabel(projectContext, record, meshLibrary, textureLibrary, audioLibrary, editorState);
                const std::string browserState = MakeAssetBrowserStateLabel(diagnostics, record, status, runtimeModeActive, referenceCount);
                const std::string displayName = MakeAssetDisplayName(record);
                const bool isSelectedAsset = IsAssetSelected(state, record.ID);

                const float rowHeight = SZ::Px(42.0f);
                const float contentOffsetY = SZ::Px(9.0f);

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                if (isSelectedAsset)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(EditorColor::AccentDim));
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::ColorConvertFloat4ToU32(EditorColor::AccentDim));
                }

                bool rowHovered = false;

                // Asset
                ImGui::TableSetColumnIndex(0);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetRowHit_Asset_" + record.ID;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                    {
                        const ImGuiIO& io = ImGui::GetIO();
                        HandleAssetSelectionClick(state, visibleAssets, record.ID, io.KeyCtrl, io.KeyShift);
                    }

                    const bool cellHovered = ImGui::IsItemHovered();
                    rowHovered |= cellHovered;

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

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + contentOffsetY));
                    ImGui::PushStyleColor(ImGuiCol_Text, isSelectedAsset ? EditorColor::Accent : EditorColor::Text);

                    ImGui::TextUnformatted(displayName.c_str());
                    ImGui::PopStyleColor();

                    if (cellHovered)
                    {
                        if (diagnostics.HasIssue())
                        {
                            std::string tooltip = MakeAssetPathDiagnosticDescription(diagnostics);
                            if (!record.Path.empty())
                                tooltip += "\n\nRegistered path: " + record.Path;
                            DrawAssetPaddedTooltip(tooltip);
                        }
                        else
                        {
                            DrawAssetPaddedTooltip(!record.Path.empty() ? record.Path : record.ID);
                        }
                    }
                }

                // Type
                ImGui::TableSetColumnIndex(1);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetRowHit_Type_" + record.ID;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                    {
                        const ImGuiIO& io = ImGui::GetIO();
                        HandleAssetSelectionClick(state, visibleAssets, record.ID, io.KeyCtrl, io.KeyShift);
                    }

                    rowHovered |= ImGui::IsItemHovered();

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

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                    DrawAssetTypePill(record.Type);
                }

                // State
                ImGui::TableSetColumnIndex(2);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetRowHit_State_" + record.ID;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                    {
                        const ImGuiIO& io = ImGui::GetIO();
                        HandleAssetSelectionClick(state, visibleAssets, record.ID, io.KeyCtrl, io.KeyShift);
                    }
                    rowHovered |= ImGui::IsItemHovered();

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

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));

                    DrawAssetStatePill(browserState);

                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    {
                        if (diagnostics.HasIssue())
                            DrawAssetPaddedTooltip(MakeAssetPathDiagnosticDescription(diagnostics));
                        else if (referenceCount > 0)
                            DrawAssetPaddedTooltip("Referenced by " + std::to_string(referenceCount) + " actor(s) in the current scene.");
                        else if (record.BuiltIn)
                            DrawAssetPaddedTooltip("Built-in engine asset.");
                        else if (runtimeModeActive)
                            DrawAssetPaddedTooltip("Asset operations are limited while runtime mode is active.");
                        else
                            DrawAssetPaddedTooltip("Project asset is currently removable.");
                    }
                }

                if (rowHovered && !isSelectedAsset)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(EditorColor::Background3));

                // Actions
                ImGui::TableSetColumnIndex(3);
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SZ::Px(4.0f));
                    const std::string inspectLabel = "Inspect##" + record.ID;

                    if (EditorButton(inspectLabel.c_str(), SZ::Px(ImVec2(82.0f, 0.0f))))
                        pendingDetailsAssetID = record.ID;
                }
            }
            ImGui::EndTable();

            DrawAssetBrowserBackgroundContextMenu(
                "##AssetBrowserListBackgroundContext",
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

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    }
}