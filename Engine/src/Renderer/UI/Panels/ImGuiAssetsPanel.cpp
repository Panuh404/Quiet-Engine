#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "ImGuiAssetFileDialog.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
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
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (record.Type != Quiet::Project::AssetType::Mesh)
        {
            return "Unknown";
        }

        if (meshLibrary.TryGet(record.ID))
        {
            return "Loaded";
        }

        if (record.BuiltIn)
        {
            return "Missing Built-In";
        }

        return "Registered";
    }

    ImVec4 MakeStatusColor(const std::string& status)
    {
        if (status == "Loaded") return {0.30f, 0.85f, 0.42f, 1.0f};
        if (status == "Registered") return {0.95f, 0.76f, 0.32f, 1.0f};
        if (status == "Missing Built-In") return {0.95f, 0.30f, 0.30f, 1.0f};
        return {0.70f, 0.72f, 0.76f, 1.0f};
    }

    void DrawSectionHeader(const char* title, const char* subtitle = nullptr)
    {
        ImGui::TextUnformatted(title);
        if (subtitle && subtitle[0] != '\0')
            ImGui::TextDisabled("%s", subtitle);
        ImGui::Separator();
    }

    bool DrawRegisterMeshForm(Quiet::Project::ProjectContext& projectContext,
                          Quiet::Editor::EditorState& editorState,
                          std::array<char, 128>& assetIDBuffer,
                          std::array<char, 260>& assetPathBuffer,
                          std::string& errorMessage)
    {
        bool registered = false;

        const std::string currentPath = assetPathBuffer.data();
        const bool hasPath = !currentPath.empty();
        const bool fileExists = hasPath && DoesAssetPathExist(projectContext, currentPath);

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
            "Assets/Meshes/model.obj",
            assetPathBuffer.data(),
            assetPathBuffer.size()
        );

        ImGui::SameLine();

        if (ImGui::Button("Browse", ImVec2(browseWidth, 0.0f)))
            Quiet::Renderer::UI::OpenMeshAssetFileDialog(projectContext);

        if (!hasPath)
            ImGui::TextDisabled("Select or paste a supported mesh file.");

        else if (fileExists)
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.42f, 1.0f), "File found");

        else
            ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "File not found");

        ImGui::Spacing();
        const bool canRegister = hasPath && fileExists;

        if (!canRegister) ImGui::BeginDisabled();
        if (ImGui::Button("Register", ImVec2(150.0f, 0.0f)))
        {
            std::string assetID = assetIDBuffer.data();
            std::string assetPath = assetPathBuffer.data();

            if (assetID.empty()) assetID = ExtractFileNameNoExt(assetPath);
            if (projectContext.RegisterMeshAsset(assetID, assetPath, false))
            {
                assetIDBuffer[0] = '\0';
                assetPathBuffer[0] = '\0';

                editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
                registered = true;
            }
            else
            {
                errorMessage =
                    "Failed to register mesh asset. Check that the Asset ID is valid.";
                ImGui::OpenPopup("Register Mesh Asset Failed");
            }
        }

        if (!canRegister) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Reload", ImVec2(150.0f, 0.0f)))
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
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildAssetsPanel(Project::ProjectContext& projectContext,
                                      Editor::EditorState& editorState,
                                      Scene::Scene& scene,
                                      const Vulkan::MeshLibrary& meshLibrary)
    {
        ImGui::Begin("Assets");
        {
            static std::array<char, 128> assetIDBuffer{};
            static std::array<char, 260> assetPathBuffer{};
            static std::string registerErrorMessage;
            static std::string removeErrorMessage;

            ImGui::TextUnformatted("Register");
            ImGui::TextDisabled("Pick a supported mesh file and register it as a project asset.");
            ImGui::Separator();

            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Scale(10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Scale(ImVec2(14.0f, 12.0f)));

            if (ImGui::BeginChild("RegisterMeshAssetCard", Scale(ImVec2(0.0f, 190.0f)), true))
            {
                DrawRegisterMeshForm(
                    projectContext,
                    editorState,
                    assetIDBuffer,
                    assetPathBuffer,
                    registerErrorMessage
                );
            }
            ImGui::EndChild();

            ImGui::PopStyleVar(3);

            if (const auto selectedMesh = DrawMeshAssetFileDialog(projectContext))
            {
                CopyToBuffer(assetPathBuffer, selectedMesh->AssetPath);
                if (assetIDBuffer[0] == '\0') CopyToBuffer(assetIDBuffer, selectedMesh->AssetID);
            }

            DrawFailurePopup("Register Mesh Asset Failed", registerErrorMessage);

            ImGui::Spacing();
            DrawSectionHeader(
                "Registry",
                "Project asset records. Built-ins are locked. External meshes can be removed."
            );

            const auto& registry = projectContext.GetAssetRegistry();
            std::string pendingRemoveAssetID;

            if (ImGui::BeginTable(
                    "AssetsTable",
                    9,
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
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, Scale(120.0f));
                ImGui::TableHeadersRow();

                const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;
                for (const auto& record : registry.Records)
                {
                    const auto stats = meshLibrary.GetStats(record.ID);
                    const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                    const std::string status = MakeStatusLabel(record, meshLibrary);

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

                    // Actions (Remove)
                    ImGui::TableSetColumnIndex(8);
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

            if (!pendingRemoveAssetID.empty())
            {
                const bool removed = projectContext.RemoveAssetByID(pendingRemoveAssetID);
                if (!removed)
                {
                    removeErrorMessage = "Failed to remove the selected asset from the registry.";
                    ImGui::OpenPopup("Remove Asset Failed");
                }
            }

            DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
        }
        ImGui::End();
    }
}