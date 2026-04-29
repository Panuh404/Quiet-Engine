#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "ImGuiFileDialog.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/MeshLibrary.h"

namespace
{
    constexpr const char* kMeshFileDialogKey = "Quiet_SelectMeshAssetFile";

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
        if (status == "Loaded")
        {
            return {0.30f, 0.85f, 0.42f, 1.0f};
        }

        if (status == "Registered")
        {
            return {0.95f, 0.76f, 0.32f, 1.0f};
        }

        if (status == "Missing Built-In")
        {
            return {0.95f, 0.30f, 0.30f, 1.0f};
        }

        return {0.70f, 0.72f, 0.76f, 1.0f};
    }

    void DrawSectionHeader(const char* title, const char* subtitle = nullptr)
    {
        ImGui::TextUnformatted(title);

        if (subtitle && subtitle[0] != '\0')
        {
            ImGui::TextDisabled("%s", subtitle);
        }

        ImGui::Separator();
    }

    void DrawPathStatus(const Quiet::Project::ProjectContext& projectContext,
                        const std::string& assetPath)
    {
        if (assetPath.empty())
        {
            ImGui::TextDisabled("Select or paste a mesh file path.");
            return;
        }

        const bool exists = DoesAssetPathExist(projectContext, assetPath);
        if (exists)
        {
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.42f, 1.0f), "File found");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "File not found");
        }
    }

    bool DrawRegisterMeshForm(Quiet::Project::ProjectContext& projectContext,
                              Quiet::Editor::EditorState& editorState,
                              std::array<char, 128>& assetIDBuffer,
                              std::array<char, 260>& assetPathBuffer,
                              std::string& errorMessage)
    {
        bool registered = false;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        ImGui::Text("Asset ID");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##AssetID", assetIDBuffer.data(), assetIDBuffer.size());

        ImGui::Text("Source Path");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##AssetPath", assetPathBuffer.data(), assetPathBuffer.size());

        ImGui::PopStyleVar(2);

        DrawPathStatus(projectContext, assetPathBuffer.data());

        if (ImGui::Button("Browse"))
        {
            IGFD::FileDialogConfig config;
            config.path = projectContext.GetRootPath().string();

            ImGuiFileDialog::Instance()->OpenDialog(
                kMeshFileDialogKey,
                "Select Mesh File",
                ".obj,.fbx,.gltf,.glb,.dae",
                config
            );
        }

        ImGui::SameLine();

        if (ImGui::Button("Register"))
        {
            std::string assetID = assetIDBuffer.data();
            std::string assetPath = assetPathBuffer.data();

            if (assetPath.empty())
            {
                errorMessage = "Source path cannot be empty.";
                ImGui::OpenPopup("Register Mesh Asset Failed");
            }
            else
            {
                if (assetID.empty())
                {
                    assetID = ExtractFileNameNoExt(assetPath);
                }

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
        }

        ImGui::SameLine();

        if (ImGui::Button("Reload Assets"))
        {
            editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Assimp imports supported mesh files on reload.");

        return registered;
    }

    void HandleMeshFileDialog(const Quiet::Project::ProjectContext& projectContext,
                              std::array<char, 128>& assetIDBuffer,
                              std::array<char, 260>& assetPathBuffer)
    {
        if (!ImGuiFileDialog::Instance()->Display(kMeshFileDialogKey))
        {
            return;
        }

        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string selectedPath = ImGuiFileDialog::Instance()->GetFilePathName();
            const std::string selectedName = ImGuiFileDialog::Instance()->GetCurrentFileName();

            const std::string normalizedPath =
                projectContext.NormalizeAssetPath(selectedPath).generic_string();

            CopyToBuffer(assetPathBuffer, normalizedPath);

            if (assetIDBuffer[0] == '\0')
            {
                CopyToBuffer(assetIDBuffer, std::filesystem::path(selectedName).stem().string());
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }

    void DrawFailurePopup(const char* title, const std::string& message)
    {
        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());

            if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildAssetsPanel(Project::ProjectContext& projectContext,
                                      Editor::EditorState& editorState,
                                      const Vulkan::MeshLibrary& meshLibrary)
    {
        ImGui::Begin("Assets");
        {
            static std::array<char, 128> assetIDBuffer{};
            static std::array<char, 260> assetPathBuffer{};
            static std::string registerErrorMessage;
            static std::string removeErrorMessage;

            DrawSectionHeader(
                "Register Mesh Asset",
                "Pick a supported mesh file, assign an asset ID, then register it in the project."
            );

            DrawRegisterMeshForm(
                projectContext,
                editorState,
                assetIDBuffer,
                assetPathBuffer,
                registerErrorMessage
            );

            HandleMeshFileDialog(projectContext, assetIDBuffer, assetPathBuffer);

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
                    5,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, Scale(120.0f));
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, Scale(120.0f));
                ImGui::TableHeadersRow();

                for (const auto& record : registry.Records)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(record.ID.c_str());

                    ImGui::TableSetColumnIndex(1);
                    const std::string typeLabel = std::string(Project::ToString(record.Type));
                    ImGui::TextUnformatted(typeLabel.c_str());

                    ImGui::TableSetColumnIndex(2);
                    if (record.Path.empty())
                    {
                        ImGui::TextDisabled("<built-in>");
                    }
                    else
                    {
                        ImGui::TextUnformatted(record.Path.c_str());
                    }

                    ImGui::TableSetColumnIndex(3);
                    const std::string status = MakeStatusLabel(record, meshLibrary);
                    ImGui::TextColored(MakeStatusColor(status), "%s", status.c_str());

                    ImGui::TableSetColumnIndex(4);
                    if (record.BuiltIn)
                    {
                        ImGui::TextDisabled("Locked");
                    }
                    else
                    {
                        const std::string buttonLabel = "Remove##" + record.ID;
                        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0.0f)))
                        {
                            pendingRemoveAssetID = record.ID;
                        }
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