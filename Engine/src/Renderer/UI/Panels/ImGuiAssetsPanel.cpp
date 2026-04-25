#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <string>

#include <imgui.h>

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

            if (ImGui::Button("Register Mesh"))
            {
                std::string assetID = assetIDBuffer.data();
                std::string assetPath = assetPathBuffer.data();

                if (projectContext.RegisterMeshAsset(assetID, assetPath, false))
                {
                    assetIDBuffer[0] = '\0';
                    assetPathBuffer[0] = '\0';
                }
            }

            ImGui::SameLine();
            ImGui::TextDisabled("Metadata only for now; importing/loading comes later.");

            ImGui::Separator();
            ImGui::Text("Registry");

            const auto& registry = projectContext.GetAssetRegistry();

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
                    ImGui::TextUnformatted(std::string(Quiet::Project::ToString(record.Type)).c_str());

                    ImGui::TableSetColumnIndex(2);
                    if (record.Path.empty())
                        ImGui::TextDisabled("<built-in>");
                    else
                        ImGui::TextUnformatted(record.Path.c_str());

                    ImGui::TableSetColumnIndex(3);
                    const std::string status = MakeStatusLabel(record, meshLibrary);
                    ImGui::TextUnformatted(status.c_str());

                    ImGui::TableSetColumnIndex(4);
                    if (record.BuiltIn)
                        ImGui::TextDisabled("Locked");
                    else
                    {
                        const std::string buttonLabel = "Remove##" + record.ID;
                        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0.0f)))
                        {
                            projectContext.RemoveAssetByID(record.ID);
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
}