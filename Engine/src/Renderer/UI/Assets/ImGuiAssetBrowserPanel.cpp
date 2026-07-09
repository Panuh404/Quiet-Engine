#include "Renderer/UI/Assets/ImGuiAssetBrowserPanel.h"

#include <algorithm>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserGridView.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserImport.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserListView.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    void DrawAssetBrowserEmptyState()
    {
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float emptyStateHeight = std::max(SZ::Px(120.0f), ImGui::GetContentRegionAvail().y);

        ImGui::BeginChild("##AssetBrowserEmptyState", ImVec2(0.0f, emptyStateHeight), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No assets match the current filters");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Clear the search field or switch the asset type filter.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    bool ShouldUseAssetDetailsSplitLayout()
    {
        return ImGui::GetContentRegionAvail().x >= SZ::Px(920.0f);
    }

    float CalculateAssetDetailsPanelWidth(const float availableWidth)
    {
        return std::clamp(
            availableWidth * 0.34f,
            SZ::Px(330.0f),
            SZ::Px(460.0f)
        );
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawAssetBrowserPanel(AssetBrowserContext& context, AssetBrowserState& state)
    {
        auto& projectContext = context.ProjectContext;
        auto& editorState = context.EditorState;
        auto& scene = context.Scene;
        const auto& meshLibrary = context.MeshLibrary;
        const auto& textureLibrary = context.TextureLibrary;

        auto& assetIDBuffer = state.AssetIDBuffer;
        auto& assetPathBuffer = state.AssetPathBuffer;
        auto& registerErrorMessage = state.ImportErrorMessage;
        auto& removeErrorMessage = state.RemoveErrorMessage;
        auto& selectedDetailsAssetID = state.SelectedDetailsAssetID;
        auto& confirmRemoveAssetID = state.ConfirmRemoveAssetID;
        auto& assetSearchBuffer = state.SearchBuffer;
        auto& assetTypeFilter = state.TypeFilter;
        auto& assetViewMode = state.ViewMode;
        auto& assetThumbnailSize = state.ThumbnailSize;

        ImGui::Begin("Assets");
        {
            const auto& registry = projectContext.GetAssetRegistry();
            bool openImportPopup = false;

            DrawAssetBrowserToolbar(
                editorState,
                registry,
                assetSearchBuffer,
                assetTypeFilter,
                assetViewMode,
                assetThumbnailSize,
                openImportPopup
            );

            if (openImportPopup) ImGui::OpenPopup("Import Asset");

            DrawImportAssetPopup(
                projectContext,
                editorState,
                assetIDBuffer,
                assetPathBuffer,
                registerErrorMessage
            );

            if (const auto selectedAsset = DrawAssetFileDialog(projectContext))
            {
                CopyToBuffer(assetPathBuffer, selectedAsset->AssetPath);
                if (assetIDBuffer[0] == '\0') CopyToBuffer(assetIDBuffer, selectedAsset->AssetID);
            }

            DrawFailurePopup("Import Asset Failed", registerErrorMessage);
            ImGui::Spacing();

            const std::vector<const Project::AssetRecord*> visibleAssets = BuildVisibleAssetList(registry, assetTypeFilter, assetSearchBuffer.data());
            bool openAssetDetailsPopup = false;

            const bool useSplitDetailsLayout = ShouldUseAssetDetailsSplitLayout();
            const ImGuiWindowFlags browserBodyFlags = useSplitDetailsLayout
                    ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                    : ImGuiWindowFlags_None;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(0.0f, 0.0f)));
            ImGui::BeginChild("##AssetBrowserBodyRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, browserBodyFlags);
            {
                std::string pendingRemoveAssetID;
                std::string pendingDetailsAssetID;
                std::string pendingReimportAssetID;

                const float assetBrowserWidth = ImGui::GetContentRegionAvail().x;
                const float detailsPanelWidth = useSplitDetailsLayout
                        ? CalculateAssetDetailsPanelWidth(assetBrowserWidth)
                        : 0.0f;

                const float tablePanelWidth = useSplitDetailsLayout
                        ? std::max(
                            SZ::Px(520.0f),
                            assetBrowserWidth -
                            detailsPanelWidth -
                            ImGui::GetStyle().ItemSpacing.x
                        )
                        : 0.0f;

                if (useSplitDetailsLayout)
                    ImGui::BeginChild("##AssetRegistryTableRegion", ImVec2(tablePanelWidth, 0.0f),
                        ImGuiChildFlags_None, ImGuiWindowFlags_None);

                if (visibleAssets.empty())
                    DrawAssetBrowserEmptyState();

                else if (assetViewMode == AssetBrowserViewMode::Browser)
                {
                    DrawAssetBrowserGrid(
                        visibleAssets,
                        projectContext,
                        editorState,
                        scene,
                        textureLibrary,
                        selectedDetailsAssetID,
                        pendingDetailsAssetID,
                        pendingReimportAssetID,
                        pendingRemoveAssetID,
                        assetThumbnailSize,
                        context.UIScale
                    );
                }
                else if (assetViewMode == AssetBrowserViewMode::List)
                {
                    DrawAssetBrowserList(
                        visibleAssets,
                        projectContext,
                        editorState,
                        scene,
                        meshLibrary,
                        textureLibrary,
                        selectedDetailsAssetID,
                        pendingDetailsAssetID,
                        pendingReimportAssetID,
                        pendingRemoveAssetID
                    );
                }

                if (useSplitDetailsLayout)
                    ImGui::EndChild();

                if (!pendingDetailsAssetID.empty())
                {
                    selectedDetailsAssetID = pendingDetailsAssetID;
                    openAssetDetailsPopup = true;
                }

                if (!pendingReimportAssetID.empty())
                {
                    selectedDetailsAssetID = pendingReimportAssetID;
                    editorState.QueueRequest(Editor::EditorRequestType::ReimportAsset, pendingReimportAssetID);
                }

                if (!pendingRemoveAssetID.empty())
                {
                    selectedDetailsAssetID = pendingRemoveAssetID;
                    confirmRemoveAssetID = pendingRemoveAssetID;
                    ImGui::OpenPopup("Remove Asset");
                }

                bool removeAssetConfirmed = false;
                bool removeAssetCancelled = false;

                DrawRemoveAssetConfirmationPopup(
                    projectContext,
                    confirmRemoveAssetID,
                    removeAssetConfirmed,
                    removeAssetCancelled,
                    context.UIScale
                );

                if (removeAssetConfirmed && !confirmRemoveAssetID.empty())
                {
                    const std::string removedAssetID = confirmRemoveAssetID;
                    const bool removed = projectContext.RemoveAssetByID(removedAssetID);

                    confirmRemoveAssetID.clear();
                    if (!removed)
                    {
                        removeErrorMessage = "Failed to remove the selected asset from the registry.";

                        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Error, "Remove failed", removeErrorMessage);
                        ImGui::OpenPopup("Remove Asset Failed");
                    }
                    else
                    {
                        if (selectedDetailsAssetID == removedAssetID)
                            selectedDetailsAssetID.clear();

                        editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
                            "Asset removed",
                            "Asset '" + removedAssetID +
                            "' was removed from the registry. The source file was not deleted."
                        );
                    }
                }
                else if (removeAssetCancelled)
                    confirmRemoveAssetID.clear();

                if (useSplitDetailsLayout)
                {
                    ImGui::SameLine();
                    ImGui::BeginChild("##AssetDetailsSideRegion", ImVec2(detailsPanelWidth, 0.0f), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                }
                else
                {
                    ImGui::Spacing();
                }

                const bool selectedAssetStillExists = selectedDetailsAssetID.empty() || Project::AssetRegistryIO::FindByID(registry, selectedDetailsAssetID) != nullptr;
                if (!selectedAssetStillExists) selectedDetailsAssetID.clear();

                DrawAssetOperationFeedback(editorState);
                DrawAssetDetailsInline(
                    projectContext,
                    meshLibrary,
                    textureLibrary,
                    scene,
                    selectedDetailsAssetID,
                    context.UIScale
                );

                if (useSplitDetailsLayout)
                {
                    ImGui::EndChild();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();

            if (openAssetDetailsPopup) ImGui::OpenPopup("Asset Details");

            DrawAssetDetailsPopup(
                projectContext,
                meshLibrary,
                textureLibrary,
                selectedDetailsAssetID,
                context.UIScale
            );

            DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
        }
        ImGui::End();
    }
}