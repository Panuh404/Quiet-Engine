#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <string>

#include <imgui.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/MeshLibrary.h"

namespace
{
    std::string MakeStatusLabel(const Quiet::Project::AssetRecord& record,
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            if (meshLibrary.TryGet(record.ID)) return "Loaded";
            if (record.BuiltIn) return "Missing Built-In";
            return "Registered";
        }
        return "Unknown";
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

            ImGui::Text("Register Mesh Asset");
            ImGui::Separator();

            ImGui::InputText("Asset ID", assetIDBuffer.data(), assetIDBuffer.size());
            ImGui::InputText("Source Path", assetPathBuffer.data(), assetPathBuffer.size());

            if (ImGui::Button("Register Mesh Asset"))
            {
                std::string assetID = assetIDBuffer.data();
                std::string assetPath = assetPathBuffer.data();

                if (projectContext.RegisterMeshAsset(assetID, assetPath, false))
                {
                    assetIDBuffer[0] = '\0';
                    assetPathBuffer[0] = '\0';

                    editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                }
                else
                {
                    ImGui::OpenPopup("Register Mesh Asset Failed");
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload Assets"))
            {
                editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
            }

            if (ImGui::BeginPopupModal("Register Mesh Asset Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("Failed to register mesh asset. Check that the Asset ID is valid.");
                if (ImGui::Button("OK"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("Supported mesh files are imported through Assimp on reload.");

            ImGui::Separator();
            ImGui::Text("Registry");

            const auto& registry = projectContext.GetAssetRegistry();

            std::string pendingRemoveAssetID;

            if (ImGui::BeginTable("AssetsTable", 5,
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Path");
                ImGui::TableSetupColumn("Status");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, Scale(110.0f));
                ImGui::TableHeadersRow();

                for (const auto& record : registry.Records)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(record.ID.c_str());

                    ImGui::TableSetColumnIndex(1);
                    const std::string typeLabel = std::string(Quiet::Project::ToString(record.Type));
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
                    ImGui::TextUnformatted(status.c_str());

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
                    ImGui::OpenPopup("Remove Asset Failed");
                }
            }
            if (ImGui::BeginPopupModal("Remove Asset Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("Failed to remove the selected asset from the registry.");
                if (ImGui::Button("OK"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}