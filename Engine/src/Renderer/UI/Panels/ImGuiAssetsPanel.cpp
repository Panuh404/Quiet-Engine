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

    static std::string ExtractFileNameNoExt(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.stem().string();
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

            ImGui::Text("Asset ID");
            ImGui::InputText("##AssetID", assetIDBuffer.data(), assetIDBuffer.size());

            ImGui::Text("Source Path");
            ImGui::InputText("##AssetPath", assetPathBuffer.data(), assetPathBuffer.size());

            if (ImGui::Button("Browse"))
            {
#ifdef _WIN32
                std::system("explorer .");
#else
                std::system("xdg-open .");
#endif
            }

            if (assetPathBuffer[0] != '\0')
            {
                std::filesystem::path testPath = assetPathBuffer.data();

                if (!testPath.is_absolute())
                {
                    testPath = projectContext.GetRootPath() / testPath;
                }

                if (std::filesystem::exists(testPath))
                {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "File found");
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "File not found");
                }
            }

            if (ImGui::Button("Register"))
            {
                std::string assetID = assetIDBuffer.data();
                std::string assetPath = assetPathBuffer.data();

                if (assetID.empty() && !assetPath.empty())
                {
                    assetID = ExtractFileNameNoExt(assetPath);
                }

                bool valid = true;
                if (assetPath.empty())
                {
                    valid = false;
                    ImGui::OpenPopup("Invalid Asset Path");
                }

                if (ImGui::BeginPopupModal("Invalid Asset Path", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::TextWrapped("Source path cannot be empty.");
                    if (ImGui::Button("OK"))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

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
            ImGui::TextDisabled("Use project-relative paths when possible, e.g. Assets/Meshes/model.obj.");

            ImGui::Separator();
            ImGui::Text("Registry");

            const auto& registry = projectContext.GetAssetRegistry();

            std::string pendingRemoveAssetID;

            if (ImGui::BeginTable("AssetsTable", 7,
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Path");
                ImGui::TableSetupColumn("Status");
                ImGui::TableSetupColumn("Vertices", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
                ImGui::TableSetupColumn("Indices", ImGuiTableColumnFlags_WidthFixed, Scale(90.0f));
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
                    if (const auto stats = meshLibrary.GetStats(record.ID))
                    {
                        ImGui::Text("%u", stats->VertexCount);
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableSetColumnIndex(5);
                    if (const auto stats = meshLibrary.GetStats(record.ID))
                    {
                        ImGui::Text("%u", stats->IndexCount);
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableSetColumnIndex(6);
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