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
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
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
        if (status == "Loaded") return {0.30f, 0.85f, 0.42f, 1.0f};
        if (status == "GPU Ready") return {0.30f, 0.85f, 0.42f, 1.0f};
        if (status == "Loaded Metadata") return {0.30f, 0.85f, 0.42f, 1.0f};
        if (status == "Registered") return {0.95f, 0.76f, 0.32f, 1.0f};
        if (status == "Missing Built-In") return {0.95f, 0.30f, 0.30f, 1.0f};
        if (status == "Missing Path") return {0.95f, 0.30f, 0.30f, 1.0f};
        return {0.70f, 0.72f, 0.76f, 1.0f};
    }

    void DrawSectionHeader(const char* title, const char* subtitle = nullptr)
    {
        ImGui::TextUnformatted(title);
        if (subtitle && subtitle[0] != '\0')
            ImGui::TextDisabled("%s", subtitle);
        ImGui::Separator();
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

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        ImGui::TextUnformatted("Asset ID");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint(
            "##AssetID",
            "Leave empty to auto-generate from filename",
            assetIDBuffer.data(),
            assetIDBuffer.size()
        );

        ImGui::TextUnformatted("Source Path");

        constexpr float browseWidth = 96.0f;
        ImGui::SetNextItemWidth(-(browseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputTextWithHint(
            "##AssetPath",
            "Assets/Meshes/model.obj or Assets/Textures/albedo.png",
            assetPathBuffer.data(),
            assetPathBuffer.size()
        );

        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(browseWidth, 0.0f)))
            Quiet::Renderer::UI::OpenAssetFileDialog(projectContext);

        if (!hasPath)
            ImGui::TextDisabled("Select or paste a supported asset file.");
        else if (!supportedType)
            ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "Unsupported asset type");
        else if (fileExists)
        {
            const std::string typeLabel = std::string(Quiet::Project::ToString(detectedType));
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.42f, 1.0f), "%s file found", typeLabel.c_str());
        }
        else
            ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "File not found");

        ImGui::Spacing();
        const bool canRegister = hasPath && supportedType && fileExists;

        if (!canRegister)
            ImGui::BeginDisabled();

        if (ImGui::Button("Register", ImVec2(125.0f, 0.0f)))
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

        if (!canRegister) ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Reload", ImVec2(125.0f, 0.0f)))
            editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);

        ImGui::PopStyleVar(2);
        return registered;
    }

    void DrawFailurePopup(const char* title, const std::string& message)
    {
        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    std::uint32_t CountMeshReferences(const Quiet::Scene::Scene& scene,
                                      const std::string& meshID)
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
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(labelWidth);

        if (value && value[0] != '\0')
            ImGui::TextUnformatted(value);
        else
            ImGui::TextDisabled("-");
    }

    void DrawDetailRowWrapped(const char* label, const std::string& value, const float labelWidth)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(labelWidth);

        if (value.empty())
        {
            ImGui::TextDisabled("-");
            return;
        }

        ImGui::TextWrapped("%s", value.c_str());
    }

    void DrawDetailRowUInt(const char* label, const std::uint32_t value, const float labelWidth)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(labelWidth);
        ImGui::Text("%u", value);
    }

    void DrawDetailRowVec3(const char* label, const glm::vec3& value, const float labelWidth)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(labelWidth);
        ImGui::Text("%.3f, %.3f, %.3f", value.x, value.y, value.z);
    }

    void DrawDetailsSectionTitle(const char* title)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted(title);
        ImGui::Separator();
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

        if (ImGui::BeginPopupModal("Asset Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
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
                            true
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
                    ImGui::TextDisabled("GPU texture resource is loaded. Shader sampling is not implemented yet.");
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            const float closeButtonWidth = 120.0f * uiScale;
            const float availableWidth = ImGui::GetContentRegionAvail().x;

            if (availableWidth > closeButtonWidth)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - closeButtonWidth);

            if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0.0f)))
                ImGui::CloseCurrentPopup();

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

            if (ImGui::BeginChild("RegisterAssetCard", Scale(ImVec2(0.0f, 190.0f)), true))
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
                "Registry",
                "Project asset records. Built-ins are locked. External assets can be removed."
            );

            const auto& registry = projectContext.GetAssetRegistry();
            std::string pendingRemoveAssetID;
            std::string pendingDetailsAssetID;

            if (ImGui::BeginTable(
                    "AssetsTable",
                    10,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.20f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Scale(80.0f));
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.40f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, Scale(110.0f));
                ImGui::TableSetupColumn("Meshes", ImGuiTableColumnFlags_WidthFixed, Scale(70.0f));
                ImGui::TableSetupColumn("Vertices", ImGuiTableColumnFlags_WidthFixed, Scale(85.0f));
                ImGui::TableSetupColumn("Indices", ImGuiTableColumnFlags_WidthFixed, Scale(85.0f));
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, Scale(55.0f));
                ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, Scale(120.0f));
                ImGui::TableHeadersRow();

                const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;
                for (const auto& record : registry.Records)
                {
                    const auto stats = meshLibrary.GetStats(record.ID);
                    const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                    const std::string status = MakeStatusLabel(record, meshLibrary, textureLibrary);

                    ImGui::TableNextRow();

                    // ID
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(record.ID.c_str());

                    // Type
                    ImGui::TableSetColumnIndex(1);
                    const std::string typeLabel = std::string(Project::ToString(record.Type));
                    ImGui::TextUnformatted(typeLabel.c_str());

                    // Path
                    ImGui::TableSetColumnIndex(2);
                    if (record.Path.empty())
                        ImGui::TextDisabled("<built-in>");
                    else
                        ImGui::TextUnformatted(record.Path.c_str());

                    // Status
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextColored(MakeStatusColor(status), "%s", status.c_str());

                    // Meshes
                    ImGui::TableSetColumnIndex(4);
                    if (stats.has_value() && stats->ImportedMeshCount > 0)
                        ImGui::Text("%u", stats->ImportedMeshCount);
                    else
                        ImGui::TextDisabled("-");

                    // Vertices
                    ImGui::TableSetColumnIndex(5);
                    if (stats.has_value())
                        ImGui::Text("%u", stats->VertexCount);
                    else
                        ImGui::TextDisabled("-");

                    // Indices
                    ImGui::TableSetColumnIndex(6);
                    if (stats.has_value())
                        ImGui::Text("%u", stats->IndexCount);
                    else
                        ImGui::TextDisabled("-");

                    // Reference Count
                    ImGui::TableSetColumnIndex(7);
                    if (referenceCount > 0)
                        ImGui::TextColored(ImVec4(0.95f, 0.76f, 0.32f, 1.0f), "%u", referenceCount);
                    else
                        ImGui::TextDisabled("0");

                    // Details
                    ImGui::TableSetColumnIndex(8);
                    const std::string detailsLabel = "Details##" + record.ID;
                    if (ImGui::Button(detailsLabel.c_str(), ImVec2(-FLT_MIN, 0.0f)))
                    {
                        pendingDetailsAssetID = record.ID;
                    }

                    // Actions (Remove)
                    ImGui::TableSetColumnIndex(9);
                    if (record.BuiltIn)
                        ImGui::TextDisabled("Locked");
                    else if (runtimeModeActive)
                    {
                        ImGui::TextDisabled("Runtime");
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            ImGui::SetTooltip("Assets cannot be removed while runtime mode is active.");
                    }
                    else if (referenceCount > 0)
                    {
                        ImGui::TextDisabled("In Use");
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            ImGui::SetTooltip("This asset is referenced by %u actor(s) in the current scene.", referenceCount);
                    }
                    else
                    {
                        const std::string buttonLabel = "Remove##" + record.ID;
                        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0.0f)))
                            pendingRemoveAssetID = record.ID;
                    }
                }
                ImGui::EndTable();
            }

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