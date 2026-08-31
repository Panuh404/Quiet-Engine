#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"

#include <algorithm>
#include <string>
#include <cstdio>

#include "Audio/AudioSubsystem.h"
#include "Audio/AudioLibrary.h"

#include <glm/vec3.hpp>
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

    static std::string FormatAudioDuration(const double seconds)
    {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%.2f s", seconds);
        return buffer;
    }

    void DrawAssetSourceDiagnostics(const Project::ProjectContext& projectContext, const Project::AssetRecord& record, const float labelWidth)
    {
        const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, record);
        DrawDetailsSectionTitle("Source");

        const char* locationLabel = diagnostics.BuiltIn
                ? "Built-in"
                : diagnostics.External
                    ? "External"
                    : "Project";

        const char* sourceState =
            diagnostics.BuiltIn
                ? "Engine managed"
                : !diagnostics.HasPath
                    ? "Missing Path"
                    : !diagnostics.Exists
                        ? "Missing File"
                        : !diagnostics.RegularFile
                            ? "Invalid Source"
                            : "Available";

        DrawDetailRow("Location", locationLabel, labelWidth);
        DrawDetailRow("Source State", sourceState, labelWidth);

        if (record.Path.empty())
            DrawDetailRow("Registry Path", record.BuiltIn ? "<built-in>" : "<missing>", labelWidth);
        else
            DrawDetailRowWrapped("Registry Path", record.Path, labelWidth);

        if (!diagnostics.ResolvedPath.empty())
            DrawDetailRowWrapped("Resolved Path", diagnostics.ResolvedPath.generic_string(), labelWidth);

        if (diagnostics.HasIssue())
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, diagnostics.IsMissing() ? EditorColor::Error : EditorColor::Warning);
            ImGui::TextWrapped("%s", MakeAssetPathDiagnosticDescription(diagnostics).c_str());
            ImGui::PopStyleColor();
        }
    }

    void DrawAssetDetailsInline(const Project::ProjectContext& projectContext,
                                const Vulkan::MeshLibrary& meshLibrary,
                                const Vulkan::TextureLibrary& textureLibrary,
                                Audio::AudioSubsystem& audioSubsystem,
                                const Scene::Scene& scene,
                                const std::string& assetID,
                                const float uiScale)
    {
        const auto& audioLibrary = audioSubsystem.GetLibrary();
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

        const std::string status = MakeStatusLabel(projectContext, *record, meshLibrary, textureLibrary, audioLibrary);
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

            const std::uint32_t referenceCount = CountAssetReferences(scene, *record);
            DrawDetailRowUInt("References", referenceCount, labelWidth);
            DrawAssetSourceDiagnostics(projectContext, *record, labelWidth);

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
            else if (record->Type == Project::AssetType::Audio)
            {
                DrawDetailsSectionTitle("Audio");

                const auto audio = audioLibrary.Get(record->ID);
                if (!audio.has_value())
                {
                    ImGui::TextDisabled("Audio metadata is not currently loaded.");
                }
                else
                {
                    DrawDetailRow("Format", audio->Encoding.c_str(), labelWidth);
                    DrawDetailRowUInt("Channels", audio->Channels, labelWidth);
                    DrawDetailRowWrapped("Sample Rate", std::to_string(audio->SampleRate) + " Hz", labelWidth);

                    if (audio->DurationValid)
                    {
                        DrawDetailRowWrapped("Duration", FormatAudioDuration(audio->DurationSeconds), labelWidth);
                    }
                    else
                    {
                        DrawDetailRow("Duration", "Unknown", labelWidth);
                    }

                    DrawDetailRowWrapped("File Size", std::to_string(audio->FileSizeBytes) + " bytes", labelWidth);
                }

                ImGui::Spacing();

                const bool metadataLoaded = audio.has_value();
                const bool previewingThisAsset = audioSubsystem.IsPreviewingAsset(record->ID);

                if (QUI::EditorButton(previewingThisAsset ? "Restart Preview" : "Preview", SZ::Px(ImVec2(120.0f, 0.0f)), metadataLoaded))
                {
                    audioSubsystem.StartPreviewAsset(record->ID);
                }

                ImGui::SameLine();

                if (EditorButton("Stop", SZ::Px(ImVec2(80.0f, 0.0f)), audioSubsystem.IsPreviewPlaying()))
                {
                    audioSubsystem.StopPreview();
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
                               Audio::AudioSubsystem& audioSubsystem,
                               const std::string& assetID,
                               const float uiScale)
    {
        const auto& audioLibrary = audioSubsystem.GetLibrary();
        if (assetID.empty()) return;

        const auto* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
        if (!record) return;

        const auto stats = meshLibrary.GetStats(record->ID);
        const std::string status = MakeStatusLabel(projectContext, *record, meshLibrary, textureLibrary, audioLibrary);

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

            DrawAssetSourceDiagnostics(projectContext, *record, labelWidth);

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
            else if (record->Type == Project::AssetType::Audio)
            {
                DrawDetailsSectionTitle("Source Audio");

                const auto audio = audioLibrary.Get(record->ID);
                if (!audio.has_value())
                {
                    ImGui::TextDisabled("Audio metadata is not loaded.");
                }
                else
                {
                    DrawDetailRow("Format", audio->Encoding.c_str(), labelWidth);
                    DrawDetailRowUInt("Channels", audio->Channels, labelWidth);
                    DrawDetailRowWrapped("Sample Rate", std::to_string(audio->SampleRate) + " Hz", labelWidth);
                    DrawDetailRowWrapped("Duration", audio->DurationValid ? FormatAudioDuration(audio->DurationSeconds) : std::string("Unknown"), labelWidth);
                    DrawDetailRowWrapped("File Size", std::to_string(audio->FileSizeBytes) + " bytes", labelWidth);
                }

                ImGui::Spacing();

                const bool previewingThisAsset = audioSubsystem.IsPreviewingAsset(record->ID);
                if (EditorButton(previewingThisAsset ? "Restart Preview" : "Preview", SZ::Px(ImVec2(120.0f, 0.0f)), audio.has_value()))
                {
                    audioSubsystem.StartPreviewAsset(record->ID);
                }

                ImGui::SameLine();

                if (EditorButton("Stop", SZ::Px(ImVec2(80.0f, 0.0f)), !audioSubsystem.GetPreviewAssetID().empty()))
                {
                    audioSubsystem.StopPreview();
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::EndPopup();
        }
    }

    void DrawAssetSelectionSummaryInline(const Project::ProjectContext &projectContext,
                                         const std::vector<std::string> &selectedAssetIDs,
                                         const std::string &primaryAssetID)
    {
        if (selectedAssetIDs.size() < 2)
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        std::uint32_t validCount = 0;
        std::uint32_t meshCount = 0;
        std::uint32_t textureCount = 0;
        std::uint32_t audioCount = 0;
        std::uint32_t scriptCount = 0;
        std::uint32_t builtInCount = 0;
        std::uint32_t issueCount = 0;
        std::uint32_t missingCount = 0;
        std::uint32_t externalCount = 0;

        for (const std::string& assetID : selectedAssetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record) continue;

            ++validCount;
            if (record->Type == Project::AssetType::Mesh) ++meshCount;
            else if (record->Type == Project::AssetType::Texture) ++textureCount;
            else if (record->Type == Project::AssetType::Audio) ++audioCount;
            else if (record->Type == Project::AssetType::Script) ++scriptCount;

            if (record->BuiltIn) ++builtInCount;

            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
            if (diagnostics.HasIssue()) ++issueCount;
            if (diagnostics.IsMissing()) ++missingCount;
            if (diagnostics.External) ++externalCount;
        }

        const float labelWidth = SZ::Px(112.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(260.0f));

        ImGui::BeginChild("##AssetMultiSelectionDetails", ImVec2(0.0f, panelHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::Text("%zu assets selected", selectedAssetIDs.size());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            DrawDetailRowUInt("Valid", validCount, labelWidth);
            DrawDetailRowUInt("Meshes", meshCount, labelWidth);
            DrawDetailRowUInt("Textures", textureCount, labelWidth);
            DrawDetailRowUInt("Audio", audioCount, labelWidth);
            DrawDetailRowUInt("Scripts", scriptCount, labelWidth);
            DrawDetailRowUInt("Built In", builtInCount, labelWidth);
            DrawDetailRowUInt("Issues", issueCount, labelWidth);
            DrawDetailRowUInt("Missing", missingCount, labelWidth);
            DrawDetailRowUInt("External", externalCount, labelWidth);
            DrawDetailRowWrapped("Primary", primaryAssetID, labelWidth);

            DrawDetailsSectionTitle("Selected Assets" );
            for (const std::string& assetID : selectedAssetIDs)
            {
                const bool primary = assetID == primaryAssetID;
                ImGui::PushStyleColor(ImGuiCol_Text, primary ? EditorColor::Accent : EditorColor::Text);
                ImGui::BulletText("%s%s", assetID.c_str(), primary ? "  (Primary)" : "");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
