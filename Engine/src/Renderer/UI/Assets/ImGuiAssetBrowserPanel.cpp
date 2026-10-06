#include "Renderer/UI/Assets/ImGuiAssetBrowserPanel.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "Audio/AudioSubsystem.h"
#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
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
#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    void DrawAssetBrowserEmptyState(const std::string& currentAssetFolder, const bool folderHasItems)
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
            ImGui::TextUnformatted(folderHasItems ? "No items match the current filters" : "This folder is empty");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text,  QUI::EditorColor::TextMuted);

            if (folderHasItems)
            {
                ImGui::TextWrapped("Clear the search field or switch the type or diagnostics filter.");
            }
            else
            {
                ImGui::TextWrapped("No physical folders or registered assets are stored directly in '%s'.", currentAssetFolder.c_str());
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
        return std::clamp(availableWidth * 0.30f, SZ::Px(320.0f), SZ::Px(420.0f));
    }

    float CalculateAssetSourcesPanelWidth(const float availableWidth)
    {
        const float preferredWidth = std::clamp(availableWidth * 0.20f, SZ::Px(220.0f), SZ::Px(300.0f));
        const float narrowWindowMaxWidth = std::max(SZ::Px(180.0f), availableWidth * 0.42f);
        return std::min(preferredWidth, narrowWindowMaxWidth);
    }

    std::string MakeBreadcrumbLabel(const std::string& breadcrumbPath, const std::string& assetsRoot)
    {
        if (breadcrumbPath == assetsRoot) return "Project";

        const std::string label = std::filesystem::path(breadcrumbPath).filename().string();
        return label.empty() ? breadcrumbPath : label;
    }

    void DrawAssetBrowserVirtualBreadcrumb(const QUI::EditorFonts& fonts)
    {
        const ImVec2 navigationPadding = SZ::Px(ImVec2(6.0f, 5.0f));
        const float navigationHeight = ImGui::GetFrameHeight() + navigationPadding.y * 2.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, navigationPadding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##AssetBrowserVirtualNavigationBar", ImVec2(0.0f, navigationHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        );
        {
            QUI::DrawToolbarButton(fonts, "AssetVirtualBack", QUI::Icons::ChevronLeft, "<", "Back", false, false, SZ::Px(ImVec2(32.0f, 0.0f)));
            ImGui::SameLine();

            QUI::DrawToolbarButton(fonts, "AssetVirtualForward", QUI::Icons::ChevronRight, ">", "Forward", false, false, SZ::Px(ImVec2(32.0f, 0.0f)));
            ImGui::SameLine();

            QUI::DrawToolbarButton(fonts, "AssetVirtualUp", QUI::Icons::ChevronUp, "Up", "Parent Folder", false, false, SZ::Px(ImVec2(32.0f, 0.0f)));
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("|");
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Accent);
            ImGui::TextUnformatted("Engine");
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("Read Only");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawAssetBrowserBreadcrumb(const Quiet::Project::ProjectContext& projectContext, Quiet::Renderer::UI::Assets::AssetBrowserState& state, const QUI::EditorFonts& fonts)
    {
        using namespace Quiet::Renderer::UI::Assets;

        if (state.CurrentSource == AssetBrowserSource::EngineBuiltIn)
        {
            DrawAssetBrowserVirtualBreadcrumb(fonts);
            return;
        }

        SynchronizeAssetFolderHistory(projectContext, state);
        SynchronizeAssetClipboard(projectContext, state);

        const std::string assetsRoot = GetAssetsRootRelativePath(projectContext);
        const bool canGoBack = CanNavigateAssetFolderBack(state);
        const bool canGoForward = CanNavigateAssetFolderForward(state);
        const bool canGoUp = state.CurrentAssetFolder != assetsRoot;

        const std::vector<std::string> breadcrumbs = BuildAssetFolderBreadcrumbs(projectContext, state.CurrentAssetFolder);
        const ImVec2 navigationPadding = SZ::Px(ImVec2(6.0f, 5.0f));
        const float navigationHeight = ImGui::GetFrameHeight() + navigationPadding.y * 2.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, navigationPadding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##AssetBrowserNavigationBar", ImVec2(0.0f, navigationHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SZ::Px(5.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, QUI::EditorColor::Background3);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, QUI::EditorColor::Background4);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::AccentDim);

            if (QUI::DrawToolbarButton(
                fonts,
                "AssetHistoryBack",
                QUI::Icons::ChevronLeft,
                "<",
                "Back",
                false,
                canGoBack,
                SZ::Px(ImVec2(32.0f, 0.0f))))
            {
                NavigateAssetFolderBack(projectContext, state);
            }

            ImGui::SameLine();

            if (QUI::DrawToolbarButton(
                fonts,
                "AssetHistoryForward",
                QUI::Icons::ChevronRight,
                ">",
                "Forward",
                false,
                canGoForward,
                SZ::Px(ImVec2(32.0f, 0.0f))))
            {
                NavigateAssetFolderForward(projectContext, state);
            }

            ImGui::SameLine();

            if (QUI::DrawToolbarButton(
                fonts,
                "AssetBreadcrumbUp",
                QUI::Icons::ChevronUp,
                "Up",
                "Parent Folder",
                false,
                canGoUp,
                SZ::Px(ImVec2(32.0f, 0.0f))))
            {
                NavigateToAssetFolder(projectContext, state, GetParentAssetFolder(projectContext, state.CurrentAssetFolder));
            }

            ImGui::PopStyleColor(3);
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("|");
            ImGui::PopStyleColor();

            for (std::size_t index = 0; index < breadcrumbs.size(); ++index)
            {
                ImGui::SameLine();

                const std::string& breadcrumbPath = breadcrumbs[index];
                const std::string label = MakeBreadcrumbLabel(breadcrumbPath, assetsRoot);
                const std::string buttonID = label + "##AssetBreadcrumb_" + breadcrumbPath;

                const float buttonWidth =
                    std::max(
                        SZ::Px(48.0f),
                        ImGui::CalcTextSize(
                            label.c_str()
                        ).x +
                        SZ::Px(20.0f)
                    );

                const bool selected = breadcrumbPath == state.CurrentAssetFolder;

                ImGui::PushStyleColor(ImGuiCol_Button, selected ? QUI::EditorColor::AccentSoft : QUI::EditorColor::Background2);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? QUI::EditorColor::AccentSoft : QUI::EditorColor::Background4);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, QUI::EditorColor::AccentDim);
                ImGui::PushStyleColor(ImGuiCol_Text, selected ? QUI::EditorColor::Text : QUI::EditorColor::TextMuted);

                if (QUI::EditorButton(buttonID.c_str(), ImVec2(buttonWidth, 0.0f)))
                    NavigateToAssetFolder(projectContext, state, breadcrumbPath);

                ImGui::PopStyleColor(4);

                if (index + 1 < breadcrumbs.size())
                {
                    ImGui::SameLine();

                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
                    ImGui::TextUnformatted(">");
                    ImGui::PopStyleColor();
                }
            }

            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void QueueAssetBrowserShortcutOperation(
        const Quiet::Project::ProjectContext& projectContext,
        const Quiet::Editor::EditorState& editorState,
        Quiet::Renderer::UI::Assets::AssetBrowserState& state,
        Quiet::Renderer::UI::Assets::AssetBrowserOperationRequest& operationRequest)
    {
        namespace Assets = Quiet::Renderer::UI::Assets;

        if (!editorState.PanelFocus.AssetBrowser) return;
        if (state.CurrentSource != Assets::AssetBrowserSource::Project) return;
        if (editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit) return;

        const ImGuiIO& io = ImGui::GetIO();

        // Preserve normal text-editing shortcuts in Search fields,
        // rename dialogs, import fields, and other text controls.
        if (io.WantTextInput) return;
        const bool hasSelection = !state.SelectedAssetIDs.empty();

        if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && state.SelectedAssetIDs.size() == 1)
        {
            const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), state.SelectedAssetIDs.front());
            if (record) RequestAssetRename(state, *record);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && hasSelection)
        {
            RequestAssetProjectDeletion(state, state.SelectedAssetIDs);
            return;
        }

        if (!io.KeyCtrl) return;

        if (ImGui::IsKeyPressed(ImGuiKey_C, false) && hasSelection)
        {
            operationRequest.Clear();
            operationRequest.Action = Assets::AssetBrowserOperationAction::CopySelection;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_X, false) && hasSelection)
        {
            operationRequest.Clear();
            operationRequest.Action = Assets::AssetBrowserOperationAction::CutSelection;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V, false) && Assets::IsAssetClipboardAvailable(projectContext, state))
        {
            operationRequest.Clear();
            operationRequest.Action = Assets::AssetBrowserOperationAction::Paste;
            operationRequest.TargetFolder = state.CurrentAssetFolder;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D, false) && hasSelection)
        {
            operationRequest.Clear();
            operationRequest.Action = Assets::AssetBrowserOperationAction::Duplicate;
            operationRequest.AssetIDs = state.SelectedAssetIDs;
            operationRequest.TargetFolder = state.CurrentAssetFolder;
        }
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

        auto& audioSubsystem = context.AudioSubsystem;
        const auto& audioLibrary = audioSubsystem.GetLibrary();

        auto& assetIDBuffer = state.AssetIDBuffer;
        auto& assetPathBuffer = state.AssetPathBuffer;
        auto& registerErrorMessage = state.ImportErrorMessage;
        auto& selectedDetailsAssetID = state.SelectedDetailsAssetID;
        auto& currentAssetFolder = state.CurrentAssetFolder;
        auto& reopenImportPopupAfterFileDialog = state.ReopenImportPopupAfterFileDialog;
        auto& assetSearchBuffer = state.SearchBuffer;
        auto& assetTypeFilter = state.TypeFilter;
        auto& assetDiagnosticsFilter = state.DiagnosticsFilter;
        auto& assetViewMode = state.ViewMode;
        auto& assetThumbnailSize = state.ThumbnailSize;
        auto& selectedContentFolder = state.SelectedContentFolder;

        SynchronizeAssetFolderHistory(projectContext, state);
        SynchronizeAssetClipboard(projectContext, state);

        ImGui::Begin("Assets");
        {
            editorState.PanelFocus.AssetBrowser = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            const auto& registry = projectContext.GetAssetRegistry();
            bool openImportPopup = false;

            AssetBrowserOperationRequest operationRequest{};

            QueueAssetBrowserShortcutOperation(projectContext, editorState, state, operationRequest);
            DrawAssetBrowserToolbar(
                editorState,
                projectContext,
                meshLibrary,
                context.Fonts,
                assetSearchBuffer,
                assetTypeFilter,
                assetDiagnosticsFilter,
                assetViewMode,
                assetThumbnailSize,
                state,
                operationRequest,
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
            ImGui::PopStyleVar();
            {
                const float bodyWidth = ImGui::GetContentRegionAvail().x;
                const float sidePanelCollapsedWidth = SZ::Px(42.0f);
                const float horizontalSpacing = ImGui::GetStyle().ItemSpacing.x;

                const float expandedSourcesPanelWidth = CalculateAssetSourcesPanelWidth(bodyWidth);
                const float actualSourcesPanelWidth = state.SourcesPanelExpanded ? expandedSourcesPanelWidth : sidePanelCollapsedWidth;
                const float widthAfterSources = std::max(0.0f, bodyWidth - actualSourcesPanelWidth - horizontalSpacing);

                const float expandedDetailsPanelWidth = useSplitDetailsLayout
                    ? CalculateAssetDetailsPanelWidth(widthAfterSources)
                    : 0.0f;

                const float detailsPanelWidth = useSplitDetailsLayout
                    ? (state.DetailsPanelExpanded ? expandedDetailsPanelWidth : sidePanelCollapsedWidth)
                    : 0.0f;

                const float contentPanelWidth = useSplitDetailsLayout
                    ? std::max(SZ::Px(1.0f), widthAfterSources - detailsPanelWidth - horizontalSpacing)
                    : 0.0f;

                AssetBrowserFolderActionRequest folderActionRequest{};
                AssetBrowserAssetMoveRequest assetMoveRequest{};

                auto DrawCurrentAssetDetails = [&]()
                {
                    if (state.SelectedAssetIDs.size() > 1)
                    {
                        DrawAssetSelectionSummaryInline(projectContext, state.SelectedAssetIDs, selectedDetailsAssetID);
                    }
                    else
                    {
                        DrawAssetDetailsInline(
                            projectContext,
                            meshLibrary,
                            textureLibrary,
                            audioSubsystem,
                            scene,
                            editorState,
                            selectedDetailsAssetID,
                            context.UIScale
                        );
                    }
                };

                DrawAssetBrowserSourcesPanel(
                    projectContext,
                    currentAssetFolder,
                    expandedSourcesPanelWidth,
                    editorState.Scene.Mode != Editor::SceneMode::Edit,
                    context.Fonts,
                    state,
                    folderActionRequest,
                    assetMoveRequest,
                    operationRequest
                );

                SynchronizeAssetFolderHistory(projectContext, state);
                SynchronizeAssetClipboard(projectContext, state);

                ImGui::SameLine();

                const ImVec2 contentRegionSize = useSplitDetailsLayout ? ImVec2(contentPanelWidth, 0.0f) : ImVec2(0.0f, 0.0f);

                ImGui::BeginChild("##AssetBrowserContentRegion", contentRegionSize, ImGuiChildFlags_AlwaysUseWindowPadding, browserBodyFlags);
                {
                    DrawAssetBrowserBreadcrumb(projectContext, state, context.Fonts);
                    ImGui::Spacing();

                    const bool projectSourceActive = state.CurrentSource == AssetBrowserSource::Project;

                    const std::vector<AssetBrowserFolderEntry> childFolders = projectSourceActive
                        ? BuildAssetChildFolderList(projectContext, currentAssetFolder)
                        : std::vector<AssetBrowserFolderEntry>{};

                    const std::vector<AssetBrowserFolderEntry> visibleFolders = BuildVisibleAssetFolderList(childFolders, assetSearchBuffer.data());

                    if (!projectSourceActive)
                    {
                        selectedContentFolder.clear();
                    }
                    else if (!selectedContentFolder.empty())
                    {
                        const bool selectedFolderStillExists =
                            std::any_of(
                                childFolders.begin(),
                                childFolders.end(),
                                [&](const AssetBrowserFolderEntry& folder)
                                {
                                    return folder.RelativePath == selectedContentFolder;
                                }
                            );

                        if (!selectedFolderStillExists)
                        {
                            selectedContentFolder.clear();
                        }
                    }

                    const std::vector<const Project::AssetRecord*> sourceAssets = projectSourceActive
                        ? BuildAssetFolderList(projectContext, registry, currentAssetFolder)
                        : BuildBuiltInAssetList(registry);

                    RepairAssetSelection(state, sourceAssets);

                    const std::vector<const Project::AssetRecord*> visibleAssets =
                        BuildVisibleAssetList(
                                projectContext,
                                sourceAssets,
                                assetTypeFilter,
                                assetDiagnosticsFilter,
                                assetSearchBuffer.data()
                            );

                    std::string pendingDetailsAssetID;
                    std::string pendingReimportAssetID;
                    std::string pendingNavigateFolder;

                    if (useSplitDetailsLayout)
                    {
                        ImGui::BeginChild("##AssetRegistryTableRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
                    }
                    const bool hasVisibleItems = !visibleFolders.empty() || !visibleAssets.empty();
                    const bool folderHasItems = !childFolders.empty() || !sourceAssets.empty();

                    if (!hasVisibleItems)
                    {
                        const std::string contentLocation = !projectSourceActive
                            ? std::string("Engine") : currentAssetFolder == GetAssetsRootRelativePath(projectContext)
                            ? std::string("Project") : currentAssetFolder;

                        DrawAssetBrowserEmptyState(contentLocation, folderHasItems);
                        // EndChild() inside DrawAssetBrowserEmptyState leaves the
                        // complete empty-state panel as the active ImGui item.
                        if (projectSourceActive)
                        {
                            const std::string emptyFolderContextMenuID = "##AssetBrowserEmptyFolderContext_" + currentAssetFolder;
                            DrawAssetFolderContextMenu(
                                emptyFolderContextMenuID.c_str(),
                                projectContext,
                                currentAssetFolder,
                                editorState.Scene.Mode != Editor::SceneMode::Edit,
                                state,
                                operationRequest,
                                folderActionRequest
                            );
                        }
                    }
                    else if (assetViewMode == AssetBrowserViewMode::Browser)
                    {
                        DrawAssetBrowserGrid(
                            visibleFolders,
                            visibleAssets,
                            projectContext,
                            editorState,
                            scene,
                            textureLibrary,
                            state,
                            pendingNavigateFolder,
                            folderActionRequest,
                            assetMoveRequest,
                            operationRequest,
                            pendingDetailsAssetID,
                            pendingReimportAssetID,
                            assetThumbnailSize);
                    }
                    else if (assetViewMode == AssetBrowserViewMode::List)
                    {
                        DrawAssetBrowserList(
                            visibleFolders,
                            visibleAssets,
                            projectContext,
                            editorState,
                            scene,
                            meshLibrary,
                            textureLibrary,
                            audioLibrary,
                            state,
                            pendingNavigateFolder,
                            folderActionRequest,
                            assetMoveRequest,
                            operationRequest,
                            pendingDetailsAssetID,
                            pendingReimportAssetID);
                    }

                    if (!pendingNavigateFolder.empty())
                        NavigateToAssetFolder(projectContext, state, pendingNavigateFolder);

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

                    const bool selectedAssetStillExists = selectedDetailsAssetID.empty() || Project::AssetRegistryIO::FindByID(registry, selectedDetailsAssetID) != nullptr;
                    if (!selectedAssetStillExists)
                    {
                        selectedDetailsAssetID.clear();
                    }

                    if (!useSplitDetailsLayout)
                    {
                        ImGui::Spacing();
                        ImGui::AlignTextToFramePadding();

                        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
                        ImGui::TextUnformatted("Details");
                        ImGui::PopStyleColor();

                        const float toggleButtonWidth = SZ::Px(32.0f);

                        ImGui::SameLine();

                        const float toggleButtonX = ImGui::GetWindowContentRegionMax().x - toggleButtonWidth;
                        if (toggleButtonX > ImGui::GetCursorPosX())
                        {
                            ImGui::SetCursorPosX(toggleButtonX);
                        }

                        const char* toggleIcon = state.DetailsPanelExpanded ? Icons::ChevronUp : Icons::ChevronDown;
                        const char* toggleFallback = state.DetailsPanelExpanded ? "^" : "v";
                        const char* toggleTooltip = state.DetailsPanelExpanded ? "Collapse Details" : "Show Details";

                        if (DrawToolbarButton(
                                context.Fonts,
                                "ToggleAssetDetails",
                                toggleIcon,
                                toggleFallback,
                                toggleTooltip,
                                false,
                                true,
                                ImVec2(toggleButtonWidth, 0.0f)))
                        {
                            state.DetailsPanelExpanded = !state.DetailsPanelExpanded;
                        }

                        if (state.DetailsPanelExpanded)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);

                            ImGui::Separator();
                            ImGui::PopStyleColor();
                            ImGui::Spacing();

                            DrawCurrentAssetDetails();
                        }
                    }
                }
                ImGui::EndChild();

                if (useSplitDetailsLayout)
                {
                    ImGui::SameLine();

                    const ImVec2 detailsPadding = state.DetailsPanelExpanded ? SZ::Px(ImVec2(10.0f, 10.0f)) : SZ::Px(ImVec2(7.0f, 10.0f));

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(6.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, detailsPadding);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
                    ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

                    ImGui::BeginChild("##AssetDetailsSideRegion", ImVec2(detailsPanelWidth, 0.0f),
                        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

                    if (!state.DetailsPanelExpanded)
                    {
                        if (DrawToolbarButton(
                                context.Fonts,
                                "ExpandAssetDetails",
                                Icons::ChevronLeft,
                                "<",
                                "Show Details",
                                false,
                                true,
                                SZ::Px(ImVec2(28.0f, 28.0f))))
                        {
                            state.DetailsPanelExpanded = true;
                        }
                    }
                    else
                    {
                        ImGui::AlignTextToFramePadding();

                        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
                        ImGui::TextUnformatted("Details");
                        ImGui::PopStyleColor();

                        const float collapseButtonWidth = SZ::Px(28.0f);

                        ImGui::SameLine();

                        const float collapseButtonX = ImGui::GetWindowContentRegionMax().x - collapseButtonWidth;
                        if (collapseButtonX > ImGui::GetCursorPosX())
                        {
                            ImGui::SetCursorPosX(collapseButtonX);
                        }

                        if (DrawToolbarButton(
                                context.Fonts,
                                "CollapseAssetDetails",
                                Icons::ChevronRight,
                                ">",
                                "Collapse Details",
                                false,
                                true,
                                ImVec2(collapseButtonWidth, 0.0f)))
                        {
                            state.DetailsPanelExpanded = false;
                        }

                        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);

                        ImGui::Separator();
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        DrawCurrentAssetDetails();
                    }

                    ImGui::EndChild();

                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar(2);
                }

                if (folderActionRequest.Action != AssetBrowserFolderAction::None)
                {
                    BeginAssetFolderAction(state, folderActionRequest);
                    const char* popupTitle = GetAssetFolderActionPopupTitle(folderActionRequest.Action);
                    if (popupTitle)
                        ImGui::OpenPopup(popupTitle);
                }

                if (assetMoveRequest.IsValid())
                {
                    ExecuteAssetMoveOperation(projectContext, editorState, state, assetMoveRequest);
                }

                if (operationRequest.IsValid())
                {
                    ExecuteAssetBrowserOperation(projectContext, editorState, state, operationRequest);
                }

                DrawAssetFolderActionPopup(projectContext, editorState, scene, state);
            }

            ImGui::EndChild();

            /*
             * Floating asset-operation feedback.
             *
             * Draw after the browser body so operations performed this frame have
             * already produced their feedback, but before modal popups so modals remain
             * visually above the notification.
             */
            DrawAssetOperationFeedback(context.Fonts, editorState);

            // POPUPS
            if (openAssetDetailsPopup)
            {
                ImGui::OpenPopup("Asset Details");
            }
            DrawAssetDetailsPopup(projectContext, meshLibrary, textureLibrary, audioSubsystem, editorState, selectedDetailsAssetID, context.UIScale);
            DrawRemoveAssetConfirmationPopup(projectContext, editorState, scene, state, context.UIScale);
            DrawDeleteAssetConfirmationPopup(projectContext, editorState, scene, state, context.UIScale);
            DrawAssetRenamePopup(projectContext, editorState, state);
        }
        ImGui::End();
    }
}