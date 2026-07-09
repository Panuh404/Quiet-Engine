#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

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
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawDetailRow(const char* label, const char* value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value && value[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }
    }

    void DrawDetailRowWrapped(const char* label, const std::string& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopStyleColor();
    }

    void DrawDetailRowUInt(const char* label, const std::uint32_t value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::Text("%u", value);
        ImGui::PopStyleColor();
    }

    void DrawDetailRowVec3(const char* label, const glm::vec3& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::Text("%.3f, %.3f, %.3f", value.x, value.y, value.z);
        ImGui::PopStyleColor();
    }

    void DrawAssetDetailsInline(const Project::ProjectContext& projectContext,
                                const Vulkan::MeshLibrary& meshLibrary,
                                const Vulkan::TextureLibrary& textureLibrary,
                                const Scene::Scene& scene,
                                const std::string& assetID,
                                const float uiScale)
    {
        if (assetID.empty())
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
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

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(260.0f));

        ImGui::BeginChild("##AssetPersistentDetails", ImVec2(0.0f, panelHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
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

            if (record->Type == Project::AssetType::Mesh)
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
            else if (record->Type == Project::AssetType::Texture)
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
                        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
                        ImGui::TextUnformatted("Dimensions");
                        ImGui::PopStyleColor();

                        ImGui::SameLine(labelWidth);

                        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
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
                        DrawTexturePreview(record->ID, textureLibrary, textureStats.value(), uiScale, 150.0f);
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawAssetDetailsPopup(const Project::ProjectContext& projectContext,
                               const Vulkan::MeshLibrary& meshLibrary,
                               const Vulkan::TextureLibrary& textureLibrary,
                               const std::string& assetID,
                               const float uiScale)
    {
        if (assetID.empty()) return;

        const auto* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
        if (!record) return;

        const auto stats = meshLibrary.GetStats(record->ID);
        const std::string status = MakeStatusLabel(*record, meshLibrary, textureLibrary);
        const ImVec4 statusColor = MakeStatusColor(status);

        const float labelWidth = 128.0f * uiScale;
        const float modalWidth = 640.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool assetDetailsOpen = true;
        if (EditorBeginPopupModal("Asset Details", assetDetailsOpen, ImGuiWindowFlags_AlwaysAutoResize))
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

            if (record->Type == Project::AssetType::Mesh)
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
            else if (record->Type == Project::AssetType::Texture)
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

                    DrawDetailRowWrapped( "File Size", std::to_string(textureStats->FileSizeBytes) + " bytes", labelWidth);
                    DrawDetailRowWrapped("Pixel Bytes", std::to_string(textureStats->PixelBytes) + " bytes", labelWidth);
                    DrawDetailRow("GPU Ready", textureStats->GpuReady ? "true" : "false", labelWidth);

                    if (textureStats->GpuReady)
                    {
                        DrawDetailsSectionTitle("Preview");
                        DrawTexturePreview(record->ID, textureLibrary, textureStats.value(), uiScale);
                    }

                    DrawDetailsSectionTitle("Texture Warnings");
                    DrawDetailRowUInt("Warnings", static_cast<std::uint32_t>(textureStats->Warnings.size()), labelWidth);

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

}