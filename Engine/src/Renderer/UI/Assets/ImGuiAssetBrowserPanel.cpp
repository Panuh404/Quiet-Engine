#include "Renderer/UI/Assets/ImGuiAssetBrowserPanel.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
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
#include "Renderer/UI/Assets/ImGuiAssetBrowserSources.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    void DrawAssetBrowserEmptyState(const std::string& currentAssetFolder, const bool folderHasAssets)
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
            ImGui::TextUnformatted(folderHasAssets ? "No assets match the current filters" : "This folder contains no registered assets");

            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);

            if (folderHasAssets)
            {
                ImGui::TextWrapped("Clear the search field or switch the asset type filter.");
            }
            else
            {
                ImGui::TextWrapped("No registered assets are stored directly in '%s'.", currentAssetFolder.c_str());
            }
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

    float CalculateAssetSourcesPanelWidth(const float availableWidth)
    {
        const float preferredWidth = std::clamp(availableWidth * 0.20f, SZ::Px(220.0f), SZ::Px(300.0f));
        const float narrowWindowMaxWidth = std::max(SZ::Px(180.0f), availableWidth * 0.42f);
        return std::min(preferredWidth, narrowWindowMaxWidth);
    }

    std::string MakeBreadcrumbLabel(const std::string& breadcrumbPath, const std::string& assetsRoot)
    {
        if (breadcrumbPath == assetsRoot) return assetsRoot;

        const std::string label = std::filesystem::path(breadcrumbPath).filename().string();
        return label.empty() ? breadcrumbPath : label;
    }

    void DrawAssetBrowserBreadcrumb(const Quiet::Project::ProjectContext& projectContext, std::string& currentAssetFolder)
    {
        using namespace Quiet::Renderer::UI::Assets;

        ValidateCurrentAssetFolder(projectContext, currentAssetFolder);
        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const bool canGoUp = currentAssetFolder != assetsRoot;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 5.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(6.0f, 4.0f)));

        if (QUI::EditorButton("Up##AssetBreadcrumbUp", SZ::Px(ImVec2(52.0f, 0.0f)), canGoUp))
            currentAssetFolder = GetParentAssetFolder(projectContext, currentAssetFolder);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted("|");
        ImGui::PopStyleColor();

        const std::vector<std::string> breadcrumbs = BuildAssetFolderBreadcrumbs(projectContext, currentAssetFolder);
        for (std::size_t index = 0; index < breadcrumbs.size(); ++index)
        {
            ImGui::SameLine();

            if (index > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                ImGui::TextUnformatted(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            const std::string& breadcrumbPath = breadcrumbs[index];
            const std::string label = MakeBreadcrumbLabel(breadcrumbPath, assetsRoot);
            const std::string buttonID = label + "##AssetBreadcrumb_" + breadcrumbPath;

            const float buttonWidth = std::max(
                SZ::Px(54.0f),
                ImGui::CalcTextSize(label.c_str()).x + SZ::Px(24.0f)
            );

            const bool selected = breadcrumbPath == currentAssetFolder;
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::Background4);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::Background4);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::Background4);
            }

            if (QUI::EditorButton(buttonID.c_str(), ImVec2(buttonWidth, 0.0f)))
                currentAssetFolder = breadcrumbPath;

            if (selected)
                ImGui::PopStyleColor(3);
        }

        ImGui::PopStyleVar(2);
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
        auto& currentAssetFolder = state.CurrentAssetFolder;
        auto& reopenImportPopupAfterFileDialog = state.ReopenImportPopupAfterFileDialog;
        auto& assetSearchBuffer = state.SearchBuffer;
        auto& assetTypeFilter = state.TypeFilter;
        auto& assetViewMode = state.ViewMode;
        auto& assetThumbnailSize = state.ThumbnailSize;

        ValidateCurrentAssetFolder(projectContext, currentAssetFolder);

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

            if (openImportPopup)
            {
                assetIDBuffer.fill('\0');
                assetPathBuffer.fill('\0');
                registerErrorMessage.clear();
                reopenImportPopupAfterFileDialog = false;
                ImGui::OpenPopup("Import Asset");
            }

            if (reopenImportPopupAfterFileDialog && !IsAssetFileDialogOpen())
            {
                ImGui::OpenPopup("Import Asset");
                reopenImportPopupAfterFileDialog = false;
            }

            DrawImportAssetPopup(projectContext, editorState, currentAssetFolder, assetIDBuffer, assetPathBuffer, registerErrorMessage, reopenImportPopupAfterFileDialog);

            if (const auto selectedAsset = DrawAssetFileDialog())
            {
                CopyToBuffer(assetPathBuffer, selectedAsset->SourcePath);
                CopyToBuffer(assetIDBuffer, selectedAsset->AssetID);
                registerErrorMessage.clear();
            }

            DrawFailurePopup("Import Asset Failed", registerErrorMessage);
            ImGui::Spacing();

            bool openAssetDetailsPopup = false;

            const bool useSplitDetailsLayout = ShouldUseAssetDetailsSplitLayout();
            const ImGuiWindowFlags browserBodyFlags = useSplitDetailsLayout
                    ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                    : ImGuiWindowFlags_None;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(0.0f, 0.0f)));
            ImGui::BeginChild("##AssetBrowserBodyRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, browserBodyFlags);
            {
                const float bodyWidth = ImGui::GetContentRegionAvail().x;
                const float sourcesPanelWidth = CalculateAssetSourcesPanelWidth(bodyWidth);

                DrawAssetBrowserSourcesPanel(projectContext, currentAssetFolder, sourcesPanelWidth);

                ImGui::SameLine();

                ImGui::BeginChild("##AssetBrowserContentRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, browserBodyFlags);
                {
                    DrawAssetBrowserBreadcrumb(projectContext, currentAssetFolder);
                    ImGui::Spacing();

                    const std::vector<const Project::AssetRecord*> folderAssets = BuildAssetFolderList(projectContext, registry, currentAssetFolder);

                    if (!selectedDetailsAssetID.empty())
                    {
                        const bool selectedAssetIsInCurrentFolder =
                            std::any_of(
                                folderAssets.begin(),
                                folderAssets.end(),
                                [&](const Project::AssetRecord* record)
                                {
                                    return record && record->ID == selectedDetailsAssetID;
                                }
                            );

                        if (!selectedAssetIsInCurrentFolder) selectedDetailsAssetID.clear();
                    }

                    const std::vector<const Project::AssetRecord*> visibleAssets = BuildVisibleAssetList(folderAssets, assetTypeFilter, assetSearchBuffer.data());

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

                    if (visibleAssets.empty()) DrawAssetBrowserEmptyState(currentAssetFolder, !folderAssets.empty());

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

                    if (useSplitDetailsLayout) ImGui::EndChild();

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