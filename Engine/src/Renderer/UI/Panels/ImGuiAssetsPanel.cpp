#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <imgui.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContext.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserGridView.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserImport.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserTypes.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

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

    void DrawDetailRow(const char* label, const char* value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value && value[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }
    }

    void DrawDetailRowWrapped(const char* label, const std::string& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopStyleColor();
    }

    void DrawDetailRowUInt(const char* label, const std::uint32_t value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::Text("%u", value);
        ImGui::PopStyleColor();
    }

    void DrawDetailRowVec3(const char* label, const glm::vec3& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::Text("%.3f, %.3f, %.3f", value.x, value.y, value.z);
        ImGui::PopStyleColor();
    }

    void DrawDetailsSectionTitle(const char* title)
    {
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    void DrawAssetDetailsPopup(const Quiet::Project::ProjectContext& projectContext,
                               const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                               const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                               const std::string& assetID,
                               const float uiScale)
    {
        if (assetID.empty()) return;

        const auto* record = Quiet::Project::AssetRegistryIO::FindByID(
            projectContext.GetAssetRegistry(),
            assetID
        );
        if (!record) return;

        const auto stats = meshLibrary.GetStats(record->ID);
        const std::string status = MakeStatusLabel(*record, meshLibrary, textureLibrary);
        const ImVec4 statusColor = MakeStatusColor(status);

        const float labelWidth = 128.0f * uiScale;
        const float modalWidth = 640.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool assetDetailsOpen = true;
        if (QUI::EditorBeginPopupModal("Asset Details", assetDetailsOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * uiScale, 7.0f * uiScale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * uiScale, 8.0f * uiScale));

            ImGui::TextUnformatted(record->ID.c_str());
            ImGui::SameLine();
            ImGui::TextColored(statusColor, "  %s", status.c_str());

            DrawDetailsSectionTitle("Asset Record");

            const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

            DrawDetailRow("ID", record->ID.c_str(), labelWidth);
            DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
            DrawDetailRow("Status", status.c_str(), labelWidth);
            DrawDetailRow("Built In", record->BuiltIn ? "true" : "false", labelWidth);

            if (record->Path.empty())
                DrawDetailRow("Path", "<built-in>", labelWidth);
            else
                DrawDetailRowWrapped("Path", record->Path, labelWidth);

            if (record->Type == Quiet::Project::AssetType::Mesh)
            {
                DrawDetailsSectionTitle("Runtime Mesh");
                if (stats.has_value())
                {
                    DrawDetailRowWrapped("Importer", stats->ImporterName, labelWidth);
                    DrawDetailRowUInt("Meshes", stats->ImportedMeshCount, labelWidth);
                    DrawDetailRowUInt("Submeshes", stats->SubmeshCount, labelWidth);
                    DrawDetailRowUInt("Materials", stats->MaterialCount, labelWidth);
                    DrawDetailRowUInt("Vertices", stats->VertexCount, labelWidth);
                    DrawDetailRowUInt("Indices", stats->IndexCount, labelWidth);
                    DrawDetailRowUInt("Warnings", stats->WarningCount, labelWidth);

                    DrawDetailsSectionTitle("Bounds");

                    if (stats->BoundsValid)
                    {
                        DrawDetailRowVec3("Min", stats->BoundsMin, labelWidth);
                        DrawDetailRowVec3("Max", stats->BoundsMax, labelWidth);

                        const glm::vec3 size = stats->BoundsMax - stats->BoundsMin;
                        DrawDetailRowVec3("Size", size, labelWidth);
                    }
                    else
                    {
                        ImGui::TextDisabled("No bounds available.");
                    }

                    DrawDetailsSectionTitle("Import Warnings");
                    if (stats->Warnings.empty())
                        ImGui::TextDisabled("No import warnings.");
                    else
                    {
                        ImGui::BeginChild(
                            "AssetImportWarnings",
                            ImVec2(0.0f, 120.0f * uiScale),
                            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                            ImGuiWindowFlags_None
                        );

                        for (const std::string& warning : stats->Warnings)
                        {
                            ImGui::BulletText("%s", warning.c_str());
                        }
                        ImGui::EndChild();
                    }
                }
                else
                    ImGui::TextDisabled("This mesh asset is not currently loaded.");
            }
            else if (record->Type == Quiet::Project::AssetType::Texture)
            {
                DrawDetailsSectionTitle("Source Texture");
                const auto textureStats = textureLibrary.GetStats(record->ID);

                if (!textureStats.has_value())
                {
                    ImGui::TextDisabled("Texture metadata is not loaded.");
                    ImGui::TextDisabled("Use Reload Meshes/Assets after registering the texture.");
                }
                else
                {
                    DrawDetailRow("Format", textureStats->Format.c_str(), labelWidth);

                    if (textureStats->DimensionsValid)
                    {
                        ImGui::TextDisabled("Dimensions");
                        ImGui::SameLine(labelWidth);
                        ImGui::Text("%u x %u", textureStats->Width, textureStats->Height);
                    }
                    else
                    {
                        DrawDetailRow("Dimensions", "Unknown", labelWidth);
                    }

                    DrawDetailRowWrapped(
                        "File Size",
                        std::to_string(textureStats->FileSizeBytes) + " bytes",
                        labelWidth
                    );
                    DrawDetailRowWrapped(
                        "Pixel Bytes",
                        std::to_string(textureStats->PixelBytes) + " bytes",
                        labelWidth
                    );
                    DrawDetailRow(
                        "GPU Ready",
                        textureStats->GpuReady ? "true" : "false",
                        labelWidth
                    );

                    if (textureStats->GpuReady)
                    {
                        DrawDetailsSectionTitle("Preview");
                        Quiet::Renderer::UI::DrawTexturePreview(
                            record->ID,
                            textureLibrary,
                            textureStats.value(),
                            uiScale
                        );
                    }

                    DrawDetailsSectionTitle("Texture Warnings");

                    DrawDetailRowUInt(
                        "Warnings",
                        static_cast<std::uint32_t>(textureStats->Warnings.size()),
                        labelWidth
                    );
                    if (textureStats->Warnings.empty())
                    {
                        ImGui::TextDisabled("No texture warnings.");
                    }
                    else
                    {
                        for (const std::string& warning : textureStats->Warnings)
                        {
                            ImGui::BulletText("%s", warning.c_str());
                        }
                    }

                    DrawDetailsSectionTitle("Runtime Texture");
                    ImGui::TextDisabled("GPU texture resource is loaded and available for material sampling.");
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::EndPopup();
        }
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

    void DrawAssetSelectionEmptyState()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(160.0f));

        ImGui::BeginChild("##AssetDetailsEmptyState", ImVec2(0.0f, panelHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No asset selected");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped(
                "Select an asset from the registry to inspect its path, runtime status, "
                "statistics, references, and preview."
            );
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawAssetDetailsInline(const Quiet::Project::ProjectContext& projectContext,
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                                const Quiet::Scene::Scene& scene,
                                const std::string& assetID,
                                const float uiScale)
    {
        if (assetID.empty())
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);

        if (!record)
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        const std::string status = MakeStatusLabel(*record, meshLibrary, textureLibrary);
        const ImVec4 statusColor = MakeStatusColor(status);
        const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

        const float labelWidth = SZ::Px(112.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(260.0f));

        ImGui::BeginChild("##AssetPersistentDetails", ImVec2(0.0f, panelHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(record->ID.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::TextColored(statusColor, "  %s", status.c_str());

            ImGui::Spacing();

            DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
            DrawDetailRow("Built In", record->BuiltIn ? "true" : "false", labelWidth);

            if (record->Path.empty())
                DrawDetailRow("Path", "<built-in>", labelWidth);
            else
                DrawDetailRowWrapped("Path", record->Path, labelWidth);

            const std::uint32_t referenceCount = CountMeshReferences(scene, record->ID);
            DrawDetailRowUInt("References", referenceCount, labelWidth);

            if (record->Type == Quiet::Project::AssetType::Mesh)
            {
                const auto stats = meshLibrary.GetStats(record->ID);
                DrawDetailsSectionTitle("Mesh");

                if (!stats.has_value())
                {
                    ImGui::TextDisabled("This mesh asset is not currently loaded.");
                }
                else
                {
                    DrawDetailRowWrapped("Importer", stats->ImporterName, labelWidth);
                    DrawDetailRowUInt("Meshes", stats->ImportedMeshCount, labelWidth);
                    DrawDetailRowUInt("Submeshes", stats->SubmeshCount, labelWidth);
                    DrawDetailRowUInt("Materials", stats->MaterialCount, labelWidth);
                    DrawDetailRowUInt("Vertices", stats->VertexCount, labelWidth);
                    DrawDetailRowUInt("Indices", stats->IndexCount, labelWidth);

                    if (stats->BoundsValid)
                    {
                        const glm::vec3 size = stats->BoundsMax - stats->BoundsMin;
                        DrawDetailRowVec3("Bounds Size", size, labelWidth);
                    }

                    if (!stats->Warnings.empty())
                    {
                        DrawDetailRowUInt("Warnings", static_cast<std::uint32_t>(stats->Warnings.size()), labelWidth);
                    }
                }
            }
            else if (record->Type == Quiet::Project::AssetType::Texture)
            {
                const auto textureStats = textureLibrary.GetStats(record->ID);
                DrawDetailsSectionTitle("Texture");

                if (!textureStats.has_value())
                {
                    ImGui::TextDisabled("Texture metadata is not currently loaded.");
                }
                else
                {
                    DrawDetailRow("Format", textureStats->Format.c_str(), labelWidth);

                    if (textureStats->DimensionsValid)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                        ImGui::TextUnformatted("Dimensions");
                        ImGui::PopStyleColor();

                        ImGui::SameLine(labelWidth);

                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                        ImGui::Text("%u x %u", textureStats->Width, textureStats->Height);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        DrawDetailRow("Dimensions", "Unknown", labelWidth);
                    }
                    DrawDetailRow("GPU Ready", textureStats->GpuReady ? "true" : "false", labelWidth);

                    if (textureStats->GpuReady)
                    {
                        ImGui::Spacing();
                        Quiet::Renderer::UI::DrawTexturePreview(
                            record->ID,
                            textureLibrary,
                            textureStats.value(),
                            uiScale,
                            150.0f
                        );
                    }
                }
            }
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

            if (openImportPopup)
            {
                ImGui::OpenPopup("Import Asset");
            }

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
            else if (assetViewMode == Assets::AssetBrowserViewMode::Browser)
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
            else
            {
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, Scale(ImVec2(10.0f, 8.0f)));

                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, QUI::EditorColor::Background2);
                ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, QUI::EditorColor::Border);
                ImGui::PushStyleColor(ImGuiCol_TableBorderLight, QUI::EditorColor::BorderSoft);
                ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.030f, 0.036f, 0.048f, 0.45f));

                if (ImGui::BeginTable("AssetsTable", 4,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 0.55f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Scale(92.0f));
                    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, Scale(190.0f));
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, Scale(96.0f));
                    ImGui::TableHeadersRow();

                    const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;

                    for (const Project::AssetRecord* recordPtr : visibleAssets)
                    {
                        if (!recordPtr) continue;

                        const Project::AssetRecord& record = *recordPtr;
                        const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                        const std::string status = MakeStatusLabel(record, meshLibrary, textureLibrary);

                        const std::string browserState =
                            MakeAssetBrowserStateLabel(
                                record,
                                status,
                                runtimeModeActive,
                                referenceCount
                            );

                        const std::string displayName = MakeAssetDisplayName(record);
                        const bool isSelectedAsset = selectedDetailsAssetID == record.ID;

                        const float rowHeight = SZ::Px(42.0f);
                        const float contentOffsetY = SZ::Px(9.0f);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                        if (isSelectedAsset)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentDim)
                            );

                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg1,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentDim)
                            );
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

                            ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + contentOffsetY));
                            ImGui::PushStyleColor(ImGuiCol_Text, isSelectedAsset ? QUI::EditorColor::Accent : QUI::EditorColor::Text);

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
                                    ImGui::SetTooltip("Referenced by %u actor(s) in the current scene.", referenceCount);

                                else if (record.BuiltIn)
                                    ImGui::SetTooltip("Built-in engine asset.");

                                else if (runtimeModeActive)
                                    ImGui::SetTooltip("Asset operations are limited while runtime mode is active.");

                                else
                                    ImGui::SetTooltip("Project asset is currently removable.");
                            }
                        }

                        if (rowHovered && !isSelectedAsset)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3)
                            );
                        }

                        // Actions
                        ImGui::TableSetColumnIndex(3);
                        {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SZ::Px(4.0f));
                            const std::string inspectLabel = "Inspect##" + record.ID;

                            if (QUI::EditorButton(inspectLabel.c_str(), Scale(ImVec2(82.0f, 0.0f))))
                                pendingDetailsAssetID = record.ID;
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }

            if (useSplitDetailsLayout)
            {
                ImGui::EndChild();
            }

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
            {
                selectedDetailsAssetID.clear();
            }

            DrawAssetOperationFeedback(editorState);
            DrawAssetDetailsInline(
                projectContext,
                meshLibrary,
                textureLibrary,
                scene,
                selectedDetailsAssetID,
                assetBrowserContext.UIScale
            );

                if (useSplitDetailsLayout)
                {
                    ImGui::EndChild();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            if (openAssetDetailsPopup)
            {
                ImGui::OpenPopup("Asset Details");
            }

            DrawAssetDetailsPopup(projectContext, meshLibrary, textureLibrary, selectedDetailsAssetID, assetBrowserContext.UIScale);
            DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
        }
        ImGui::End();
    }
}