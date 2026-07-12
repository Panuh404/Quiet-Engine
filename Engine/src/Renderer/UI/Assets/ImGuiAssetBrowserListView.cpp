#include "Renderer/UI/Assets/ImGuiAssetBrowserListView.h"

#include <string>
#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserList(
        const std::vector<const Project::AssetRecord*>& visibleAssets,
        const Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        std::string& selectedDetailsAssetID,
        std::string& pendingDetailsAssetID,
        std::string& pendingReimportAssetID,
        std::string& pendingRemoveAssetID)
    {
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

            for (const Project::AssetRecord* recordPtr : visibleAssets)
            {
                if (!recordPtr) continue;
                const Project::AssetRecord& record = *recordPtr;
                const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                const std::string status = MakeStatusLabel(record, meshLibrary, textureLibrary);

                const std::string browserState = MakeAssetBrowserStateLabel(record, status, runtimeModeActive, referenceCount);

                const std::string displayName = MakeAssetDisplayName(record);
                const bool isSelectedAsset = selectedDetailsAssetID == record.ID;

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
                        selectedDetailsAssetID = record.ID;

                    const bool cellHovered = ImGui::IsItemHovered();
                    rowHovered |= cellHovered;

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

                    DrawAssetBrowserDragSource(
                        record,
                        !runtimeModeActive
                    );

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + contentOffsetY));
                    ImGui::PushStyleColor(ImGuiCol_Text, isSelectedAsset ? EditorColor::Accent : EditorColor::Text);

                    ImGui::TextUnformatted(displayName.c_str());
                    ImGui::PopStyleColor();

                    if (cellHovered && !record.Path.empty())
                        DrawAssetPaddedTooltip(record.Path);
                    else if (cellHovered)
                        DrawAssetPaddedTooltip(record.ID);
                }

                // Type
                ImGui::TableSetColumnIndex(1);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetRowHit_Type_" + record.ID;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                        selectedDetailsAssetID = record.ID;

                    rowHovered |= ImGui::IsItemHovered();

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

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                    DrawAssetTypePill(record.Type);
                }

                // State
                ImGui::TableSetColumnIndex(2);
                {
                    const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                    const std::string hitID = "##AssetRowHit_State_" + record.ID;

                    if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                        selectedDetailsAssetID = record.ID;

                    rowHovered |= ImGui::IsItemHovered();

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

                    ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));

                    DrawAssetStatePill(browserState);

                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    {
                        if (referenceCount > 0)
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
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    }
}