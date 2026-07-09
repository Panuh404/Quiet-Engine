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


    void DrawFailurePopup(const char* title, const std::string& message)
    {
        bool popupOpen = true;
        if (QUI::EditorBeginPopupModal(title, popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::Spacing();

            if (QUI::EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    ImVec4 MakeAssetFeedbackTextColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success: return QUI::EditorColor::Success;
            case Quiet::Editor::EditorFeedbackSeverity::Warning: return QUI::EditorColor::Warning;
            case Quiet::Editor::EditorFeedbackSeverity::Error: return QUI::EditorColor::Error;
            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default: return QUI::EditorColor::Accent;
        }
    }

    ImVec4 MakeAssetFeedbackBackgroundColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success: return {0.035f, 0.105f, 0.065f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Warning: return {0.130f, 0.095f, 0.030f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Error: return {0.140f, 0.045f, 0.050f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default: return {0.040f, 0.075f, 0.135f, 1.0f};
        }
    }

    void DrawAssetOperationFeedback(Quiet::Editor::EditorState& editorState)
    {
        auto& feedback = editorState.AssetOperationFeedback;
        if (!feedback.Visible) return;

        const ImVec4 accentColor = MakeAssetFeedbackTextColor(feedback.Severity);
        const ImVec4 backgroundColor = MakeAssetFeedbackBackgroundColor(feedback.Severity);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Border, accentColor);

        ImGui::BeginChild("##AssetOperationFeedback", ImVec2(0.0f, SZ::Px(74.0f)),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            const std::string title = feedback.Title.empty() ? "Asset operation" : feedback.Title;
            const std::string message = feedback.Message.empty() ? "Operation completed." : feedback.Message;

            ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            const float dismissWidth = SZ::Px(82.0f);
            const float dismissX = ImGui::GetWindowContentRegionMax().x - dismissWidth;

            ImGui::SameLine();

            if (dismissX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(dismissX);

            if (QUI::EditorButton("Dismiss", ImVec2(dismissWidth, 0.0f)))
                feedback.Clear();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
    }

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

    void DrawRemoveAssetConfirmationPopup(const Quiet::Project::ProjectContext& projectContext,
                                          const std::string& assetID,
                                          bool& confirmed,
                                          bool& cancelled,
                                          const float uiScale)
    {
        confirmed = false;
        cancelled = false;

        if (assetID.empty()) return;

        const float modalWidth = 560.0f * uiScale;
        const float labelWidth = 92.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool removeAssetOpen = true;
        if (!QUI::EditorBeginPopupModal("Remove Asset", removeAssetOpen, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
        if (!record)
        {
            ImGui::TextWrapped("The selected asset is no longer registered.");
            ImGui::Spacing();

            if (QUI::EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
            {
                cancelled = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Warning);
        ImGui::TextUnformatted("Remove asset from registry?");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::TextWrapped(
            "This will unregister the asset from the current project. "
            "The source file on disk will not be deleted."
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

        DrawDetailRow("ID", record->ID.c_str(), labelWidth);
        DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
        DrawDetailRow("Source", record->BuiltIn ? "Built-in" : "Project", labelWidth);

        if (record->Path.empty())
            DrawDetailRow("Path", "<none>", labelWidth);
        else
            DrawDetailRowWrapped("Path", record->Path, labelWidth);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (QUI::EditorButton("Cancel", SZ::Px(ImVec2(130.0f, 0.0f))))
        {
            cancelled = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        if (QUI::EditorButton("Remove Asset", SZ::Px(ImVec2(160.0f, 0.0f))))
        {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
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