#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

#include <glm/vec3.hpp>
#include <imgui.h>

#include "ImGuiAssetFileDialog.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    template <std::size_t Size>
    void CopyToBuffer(std::array<char, Size>& buffer, const std::string& value)
    {
        std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    }

    std::string ExtractFileNameNoExt(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    std::filesystem::path ResolveDisplayPath(const Quiet::Project::ProjectContext& projectContext,
                                             const std::string& assetPath)
    {
        std::filesystem::path path = assetPath;
        if (path.empty())
        {
            return {};
        }

        if (path.is_absolute())
        {
            return path.lexically_normal();
        }

        return (projectContext.GetRootPath() / path).lexically_normal();
    }

    bool DoesAssetPathExist(const Quiet::Project::ProjectContext& projectContext,
                            const std::string& assetPath)
    {
        const std::filesystem::path resolvedPath = ResolveDisplayPath(projectContext, assetPath);
        return !resolvedPath.empty() && std::filesystem::exists(resolvedPath);
    }

    std::string MakeStatusLabel(const Quiet::Project::AssetRecord& record,
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary)
    {
        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            if (meshLibrary.TryGet(record.ID))
                return "Loaded";

            if (record.BuiltIn)
                return "Missing Built-In";

            return "Registered";
        }

        if (record.Type == Quiet::Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value())
                return textureStats->GpuReady ? "GPU Ready" : "Loaded Metadata";

            if (record.Path.empty() && !record.BuiltIn)
                return "Missing Path";

            return "Registered";
        }

        return "Unknown";
    }

    ImVec4 MakeStatusColor(const std::string& status)
    {
        if (status == "Loaded") return QUI::EditorColor::Success;
        if (status == "GPU Ready") return QUI::EditorColor::Success;
        if (status == "Loaded Metadata") return QUI::EditorColor::Success;
        if (status == "Registered") return QUI::EditorColor::Warning;
        if (status == "Missing Built-In") return QUI::EditorColor::Error;
        if (status == "Missing Path") return QUI::EditorColor::Error;
        return QUI::EditorColor::TextMuted;
    }

    void DrawPillLabel(const char* text, const ImVec4& textColor, const ImVec4& backgroundColor)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 padding = SZ::Px(ImVec2(8.0f, 3.0f));
        const float rounding = SZ::Px(6.0f);

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(
            min.x + textSize.x + padding.x * 2.0f,
            min.y + textSize.y + padding.y * 2.0f
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            rounding
        );

        drawList->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            rounding
        );

        drawList->AddText(
            ImVec2(min.x + padding.x, min.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(textColor),
            text
        );

        ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
    }

    void DrawAssetTypePill(const Quiet::Project::AssetType type)
    {
        const std::string typeLabel = std::string(Quiet::Project::ToString(type));

        ImVec4 background = QUI::EditorColor::Background3;
        ImVec4 text = QUI::EditorColor::TextMuted;

        if (type == Quiet::Project::AssetType::Mesh)
        {
            background = ImVec4(0.045f, 0.100f, 0.075f, 1.0f);
            text = QUI::EditorColor::Success;
        }
        else if (type == Quiet::Project::AssetType::Texture)
        {
            background = ImVec4(0.075f, 0.055f, 0.115f, 1.0f);
            text = ImVec4(0.760f, 0.620f, 1.000f, 1.0f);
        }
        DrawPillLabel(typeLabel.c_str(), text, background);
    }

    void DrawAssetStatusPill(const std::string& status)
    {
        const ImVec4 statusColor = MakeStatusColor(status);
        ImVec4 background = QUI::EditorColor::Background3;

        if (status == "Loaded" || status == "GPU Ready" || status == "Loaded Metadata")
            background = ImVec4(0.035f, 0.105f, 0.065f, 1.0f);
        else if (status == "Registered")
            background = ImVec4(0.130f, 0.095f, 0.030f, 1.0f);
        else if (status == "Missing Built-In" || status == "Missing Path")
            background = ImVec4(0.140f, 0.045f, 0.050f, 1.0f);

        DrawPillLabel(status.c_str(), statusColor, background);
    }

    void DrawAssetMutedText(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void DrawAssetDimText(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void DrawSectionHeader(const char* title, const char* subtitle = nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        if (subtitle && subtitle[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    bool DrawRegisterAssetForm(Quiet::Project::ProjectContext& projectContext,
                               Quiet::Editor::EditorState& editorState,
                               std::array<char, 128>& assetIDBuffer,
                               std::array<char, 260>& assetPathBuffer,
                               std::string& errorMessage)
    {
        bool registered = false;

        const std::string currentPath = assetPathBuffer.data();
        const bool hasPath = !currentPath.empty();
        const bool fileExists = hasPath && DoesAssetPathExist(projectContext, currentPath);

        const Quiet::Project::AssetType detectedType =
            hasPath
                ? Quiet::Renderer::UI::DetectImportAssetType(currentPath)
                : Quiet::Project::AssetType::Unknown;

        const bool supportedType = detectedType != Quiet::Project::AssetType::Unknown;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::TextUnformatted("Asset ID");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint(
            "##AssetID",
            "Leave empty to auto-generate from filename",
            assetIDBuffer.data(),
            assetIDBuffer.size()
        );

        ImGui::TextUnformatted("Source Path");

        const float browseWidth = SZ::Px(96.0f);
        ImGui::SetNextItemWidth(-(browseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputTextWithHint(
            "##AssetPath",
            "Assets/Meshes/model.obj or Assets/Textures/albedo.png",
            assetPathBuffer.data(),
            assetPathBuffer.size()
        );

        ImGui::SameLine();
        if (QUI::EditorButton("Browse", ImVec2(browseWidth, 0.0f)))
            Quiet::Renderer::UI::OpenAssetFileDialog(projectContext);

        if (!hasPath)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Select or paste a supported asset file.");
            ImGui::PopStyleColor();
        }
        else if (!supportedType)
        {
            ImGui::TextColored(QUI::EditorColor::Error, "Unsupported asset type");
        }
        else if (fileExists)
        {
            const std::string typeLabel = std::string(Quiet::Project::ToString(detectedType));
            ImGui::TextColored(QUI::EditorColor::Success, "%s file found", typeLabel.c_str());
        }
        else
        {
            ImGui::TextColored(QUI::EditorColor::Error, "File not found");
        }

        ImGui::Spacing();
        const bool canRegister = hasPath && supportedType && fileExists;

        if (QUI::EditorButton("Register", SZ::Px(ImVec2(125.0f, 0.0f)), canRegister))
        {
            std::string assetID = assetIDBuffer.data();
            std::string assetPath = assetPathBuffer.data();

            if (assetID.empty()) assetID = ExtractFileNameNoExt(assetPath);
            bool success = false;

            switch (detectedType)
            {
                case Quiet::Project::AssetType::Mesh:
                    success = projectContext.RegisterMeshAsset(assetID, assetPath, false);
                    if (success)
                        editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
                    break;

                case Quiet::Project::AssetType::Texture:
                    success = projectContext.RegisterTextureAsset(assetID, assetPath, false);
                    if (success)
                        editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
                    break;

                case Quiet::Project::AssetType::Unknown:
                default:
                    success = false;
                    break;
            }

            if (success)
            {
                assetIDBuffer[0] = '\0';
                assetPathBuffer[0] = '\0';
                registered = true;
            }
            else
            {
                errorMessage =
                    "Failed to register asset. Check that the Asset ID is valid and the file type is supported.";
                ImGui::OpenPopup("Register Asset Failed");
            }
        }
        ImGui::SameLine();

        if (QUI::EditorButton("Reload", SZ::Px(ImVec2(125.0f, 0.0f))))
            editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);

        ImGui::PopStyleVar(2);
        return registered;
    }

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

    std::uint32_t CountMeshReferences(const Quiet::Scene::Scene& scene, const std::string& meshID)
    {
        std::uint32_t count = 0;
        for (const auto& actor : scene.GetActors())
        {
            if (!actor.StaticMesh.has_value()) continue;
            if (actor.StaticMesh->MeshResourceID == meshID) ++count;
        }
        return count;
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
            static std::array<char, 128> assetIDBuffer{};
            static std::array<char, 260> assetPathBuffer{};
            static std::string registerErrorMessage;
            static std::string removeErrorMessage;
            static std::string selectedDetailsAssetID;

            ImGui::TextUnformatted("Register");
            ImGui::TextDisabled("Pick a supported file and register it as a project asset.");
            ImGui::Separator();

            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Scale(10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Scale(ImVec2(14.0f, 12.0f)));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
            ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

            if (ImGui::BeginChild(
                "RegisterAssetCard",
                Scale(ImVec2(0.0f, 210.0f)),
                ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                DrawRegisterAssetForm(
                    projectContext,
                    editorState,
                    assetIDBuffer,
                    assetPathBuffer,
                    registerErrorMessage
                );
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);

            if (const auto selectedAsset = DrawAssetFileDialog(projectContext))
            {
                CopyToBuffer(assetPathBuffer, selectedAsset->AssetPath);
                if (assetIDBuffer[0] == '\0')
                    CopyToBuffer(assetIDBuffer, selectedAsset->AssetID);
            }

            DrawFailurePopup("Register Asset Failed", registerErrorMessage);

            ImGui::Spacing();
            DrawSectionHeader(
                "Asset Registry",
                "Project assets available to scenes and materials."
            );

            const auto& registry = projectContext.GetAssetRegistry();
            std::string pendingRemoveAssetID;
            std::string pendingDetailsAssetID;

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, Scale(ImVec2(10.0f, 8.0f)));

            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, QUI::EditorColor::Background2);
            ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, QUI::EditorColor::Border);
            ImGui::PushStyleColor(ImGuiCol_TableBorderLight, QUI::EditorColor::BorderSoft);
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.030f, 0.036f, 0.048f, 0.45f));

            if (ImGui::BeginTable(
                    "AssetsTable",
                    6,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
                ImGui::TableSetupColumn("Stats", ImGuiTableColumnFlags_WidthStretch, 0.30f);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, Scale(60.0f));
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, Scale(160.0f));
                ImGui::TableHeadersRow();

                const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;

                for (const auto& record : registry.Records)
                {
                    const auto stats = meshLibrary.GetStats(record.ID);
                    const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                    const std::string status = MakeStatusLabel(record, meshLibrary, textureLibrary);

                    ImGui::TableNextRow();

                    // Asset
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                    ImGui::TextUnformatted(record.ID.c_str());
                    ImGui::PopStyleColor();

                    if (record.Path.empty())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
                        ImGui::TextUnformatted("<built-in>");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                        ImGui::TextWrapped("%s", record.Path.c_str());
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        {
                            ImGui::SetTooltip("%s", record.Path.c_str());
                        }
                    }

                    // Type
                    ImGui::TableSetColumnIndex(1);
                    DrawAssetTypePill(record.Type);

                    // Status
                    ImGui::TableSetColumnIndex(2);
                    DrawAssetStatusPill(status);

                    // Stats
                    ImGui::TableSetColumnIndex(3);
                    if (stats.has_value())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                        ImGui::Text(
                            "%u mesh%s  |  %u vertices  |  %u indices",
                            stats->ImportedMeshCount,
                            stats->ImportedMeshCount == 1 ? "" : "es",
                            stats->VertexCount,
                            stats->IndexCount
                        );
                        ImGui::PopStyleColor();
                    }
                    else if (record.Type == Project::AssetType::Texture)
                    {
                        const auto textureStats = textureLibrary.GetStats(record.ID);
                        if (textureStats.has_value() && textureStats->DimensionsValid)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                            ImGui::Text(
                                "%u x %u  |  %s",
                                textureStats->Width,
                                textureStats->Height,
                                textureStats->GpuReady ? "GPU ready" : "metadata"
                            );
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            DrawAssetDimText("-");
                        }
                    }
                    else
                    {
                        DrawAssetDimText("-");
                    }

                    // Refs
                    ImGui::TableSetColumnIndex(4);
                    if (referenceCount > 0)
                    {
                        ImGui::TextColored(QUI::EditorColor::Warning, "%u", referenceCount);
                    }
                    else
                    {
                        DrawAssetDimText("0");
                    }

                    // Actions
                    ImGui::TableSetColumnIndex(5);

                    const std::string inspectLabel = "Inspect##" + record.ID;
                    if (QUI::EditorButton(inspectLabel.c_str(), Scale(ImVec2(82.0f, 0.0f))))
                    {
                        pendingDetailsAssetID = record.ID;
                    }

                    ImGui::SameLine();

                    if (record.BuiltIn)
                    {
                        DrawAssetMutedText("Locked");
                    }
                    else if (runtimeModeActive)
                    {
                        DrawAssetMutedText("Runtime");

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        {
                            ImGui::SetTooltip("Assets cannot be removed while runtime mode is active.");
                        }
                    }
                    else if (referenceCount > 0)
                    {
                        DrawAssetMutedText("In Use");

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        {
                            ImGui::SetTooltip(
                                "This asset is referenced by %u actor(s) in the current scene.",
                                referenceCount
                            );
                        }
                    }
                    else
                    {
                        const std::string removeLabel = "Remove##" + record.ID;

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.120f, 0.045f, 0.052f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.220f, 0.060f, 0.075f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.330f, 0.080f, 0.095f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);

                        if (QUI::EditorButton(removeLabel.c_str(), Scale(ImVec2(82.0f, 0.0f))))
                        {
                            pendingRemoveAssetID = record.ID;
                        }

                        ImGui::PopStyleColor(4);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();

            if (!pendingDetailsAssetID.empty())
            {
                selectedDetailsAssetID = pendingDetailsAssetID;
                ImGui::OpenPopup("Asset Details");
            }
            if (!pendingRemoveAssetID.empty())
            {
                const bool removed = projectContext.RemoveAssetByID(pendingRemoveAssetID);
                if (!removed)
                {
                    removeErrorMessage = "Failed to remove the selected asset from the registry.";
                    ImGui::OpenPopup("Remove Asset Failed");
                }
            }
            DrawAssetDetailsPopup(projectContext, meshLibrary, textureLibrary, selectedDetailsAssetID, GetUIScale());
            DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
        }
        ImGui::End();
    }
}