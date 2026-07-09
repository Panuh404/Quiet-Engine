#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserGridView.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserImport.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserListView.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace QUIA = Quiet::Renderer::UI::Assets;

    using QUIA::AssetBrowserState;
    using QUIA::AssetBrowserThumbnailSize;
    using QUIA::AssetBrowserTypeFilter;
    using QUIA::AssetBrowserViewMode;
    using QUIA::AssetBrowserContext;

    using namespace Quiet::Renderer::UI::Assets;


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

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildAssetsPanel(Project::ProjectContext& projectContext,
                                      Editor::EditorState& editorState,
                                      Scene::Scene& scene,
                                      const Vulkan::MeshLibrary& meshLibrary,
                                      const Vulkan::TextureLibrary& textureLibrary)
    {
        ImGui::Begin("Assets");
        {
            static AssetBrowserState assetBrowserState{};

            auto& assetIDBuffer = assetBrowserState.AssetIDBuffer;
            auto& assetPathBuffer = assetBrowserState.AssetPathBuffer;
            auto& registerErrorMessage = assetBrowserState.ImportErrorMessage;
            auto& removeErrorMessage = assetBrowserState.RemoveErrorMessage;
            auto& selectedDetailsAssetID = assetBrowserState.SelectedDetailsAssetID;
            auto& confirmRemoveAssetID = assetBrowserState.ConfirmRemoveAssetID;
            auto& assetSearchBuffer = assetBrowserState.SearchBuffer;
            auto& assetTypeFilter = assetBrowserState.TypeFilter;
            auto& assetViewMode = assetBrowserState.ViewMode;
            auto& assetThumbnailSize = assetBrowserState.ThumbnailSize;

            const AssetBrowserContext assetBrowserContext{
                projectContext,
                editorState,
                scene,
                meshLibrary,
                textureLibrary,
                GetUIScale()
            };

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
                if (assetIDBuffer[0] == '\0')
                {
                    CopyToBuffer(assetIDBuffer, selectedAsset->AssetID);
                }
            }

            DrawFailurePopup("Import Asset Failed", registerErrorMessage);

            ImGui::Spacing();
            const std::vector<const Project::AssetRecord*> visibleAssets =
                BuildVisibleAssetList(registry, assetTypeFilter, assetSearchBuffer.data());

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
                        assetBrowserWidth - detailsPanelWidth - ImGui::GetStyle().ItemSpacing.x
                    )
                    : 0.0f;

                if (useSplitDetailsLayout)
                {
                    ImGui::BeginChild("##AssetRegistryTableRegion", ImVec2(tablePanelWidth, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
                }

                if (visibleAssets.empty())
                {
                    DrawAssetBrowserEmptyState();
                }
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
                        assetBrowserContext.UIScale
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
                    editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReimportAsset, pendingReimportAssetID);
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
                    assetBrowserContext.UIScale
                );

                if (removeAssetConfirmed && !confirmRemoveAssetID.empty())
                {
                    const std::string removedAssetID = confirmRemoveAssetID;
                    const bool removed = projectContext.RemoveAssetByID(removedAssetID);

                    confirmRemoveAssetID.clear();

                    if (!removed)
                    {
                        removeErrorMessage = "Failed to remove the selected asset from the registry.";

                        editorState.AssetOperationFeedback.Set(
                            Editor::EditorFeedbackSeverity::Error,
                            "Remove failed",
                            removeErrorMessage
                        );

                        ImGui::OpenPopup("Remove Asset Failed");
                    }
                    else
                    {
                        if (selectedDetailsAssetID == removedAssetID)
                        {
                            selectedDetailsAssetID.clear();
                        }

                        editorState.AssetOperationFeedback.Set(
                            Editor::EditorFeedbackSeverity::Success,
                            "Asset removed",
                            "Asset '" + removedAssetID + "' was removed from the registry. The source file was not deleted."
                        );
                    }
                }
                else if (removeAssetCancelled)
                {
                    confirmRemoveAssetID.clear();
                }
                if (useSplitDetailsLayout)
                {
                    ImGui::SameLine();
                    ImGui::BeginChild("##AssetDetailsSideRegion", ImVec2(detailsPanelWidth, 0.0f),
                        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                }
                else
                {
                    ImGui::Spacing();
                }

                const bool selectedAssetStillExists = selectedDetailsAssetID.empty() ||
                    Project::AssetRegistryIO::FindByID(registry, selectedDetailsAssetID) != nullptr;

                if (!selectedAssetStillExists)
                    selectedDetailsAssetID.clear();

                DrawAssetOperationFeedback(editorState);
                DrawAssetDetailsInline(
                    projectContext,
                    meshLibrary,
                    textureLibrary,
                    scene,
                    selectedDetailsAssetID,
                    assetBrowserContext.UIScale
                );

                if (useSplitDetailsLayout) ImGui::EndChild();

                ImGui::EndChild();
                ImGui::PopStyleVar();

                if (openAssetDetailsPopup)
                    ImGui::OpenPopup("Asset Details");

                DrawAssetDetailsPopup(projectContext, meshLibrary, textureLibrary, selectedDetailsAssetID, assetBrowserContext.UIScale);
                DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
            }
            ImGui::End();
        }
    }
}