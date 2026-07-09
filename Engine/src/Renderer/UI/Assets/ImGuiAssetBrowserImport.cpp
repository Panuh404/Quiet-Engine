#include "Renderer/UI/Assets/ImGuiAssetBrowserImport.h"

#include <filesystem>
#include <string>

#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
}

namespace Quiet::Renderer::UI::Assets
{
    bool DrawRegisterAssetForm(Project::ProjectContext& projectContext,
                               Editor::EditorState& editorState,
                               std::array<char, 128>& assetIDBuffer,
                               std::array<char, 260>& assetPathBuffer,
                               std::string& errorMessage)
    {
        bool registered = false;

        const std::string currentPath = assetPathBuffer.data();
        const bool hasPath = !currentPath.empty();
        const bool fileExists = hasPath && DoesAssetPathExist(projectContext, currentPath);

        const Project::AssetType detectedType =
            hasPath
                ? DetectImportAssetType(currentPath)
                : Project::AssetType::Unknown;

        const bool supportedType = detectedType != Project::AssetType::Unknown;

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
        if (EditorButton("Browse", ImVec2(browseWidth, 0.0f)))
            OpenAssetFileDialog(projectContext);

        if (!hasPath)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextUnformatted("Select or paste a supported asset file.");
            ImGui::PopStyleColor();
        }
        else if (!supportedType)
        {
            ImGui::TextColored(EditorColor::Error, "Unsupported asset type");
        }
        else if (fileExists)
        {
            const std::string typeLabel = std::string(Quiet::Project::ToString(detectedType));
            ImGui::TextColored(EditorColor::Success, "%s file found", typeLabel.c_str());
        }
        else
        {
            ImGui::TextColored(EditorColor::Error, "File not found");
        }

        ImGui::Spacing();
        const bool canRegister = hasPath && supportedType && fileExists;

        if (EditorButton("Import", SZ::Px(ImVec2(125.0f, 0.0f)), canRegister))
        {
            std::string assetID = assetIDBuffer.data();
            std::string assetPath = assetPathBuffer.data();

            if (assetID.empty()) assetID = ExtractFileNameNoExt(assetPath);
            bool success = false;

            switch (detectedType)
            {
                case Project::AssetType::Mesh:
                    success = projectContext.RegisterMeshAsset(assetID, assetPath, false);
                    if (success) editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                    break;

                case Project::AssetType::Texture:
                    success = projectContext.RegisterTextureAsset(assetID, assetPath, false);
                    if (success) editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                    break;

                case Project::AssetType::Unknown:
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
                errorMessage = "Failed to import asset. Check that the Asset ID is valid and the file type is supported.";
                ImGui::OpenPopup("Import Asset Failed");
            }
        }

        ImGui::PopStyleVar(2);
        return registered;
    }

    void DrawImportAssetPopup(Project::ProjectContext& projectContext,
                              Editor::EditorState& editorState,
                              std::array<char, 128>& assetIDBuffer,
                              std::array<char, 260>& assetPathBuffer,
                              std::string& errorMessage)
    {
        ImGui::SetNextWindowSize(SZ::Px(ImVec2(640.0f, 0.0f)), ImGuiCond_Appearing);
        bool importPopupOpen = true;

        if (!EditorBeginPopupModal("Import Asset", importPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        // Title
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextUnformatted("Import Asset");
        ImGui::PopStyleColor();

        // Subtitle
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("%s", "Register a supported mesh or texture file as a project asset.");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        if (DrawRegisterAssetForm(projectContext, editorState, assetIDBuffer, assetPathBuffer, errorMessage))
        {
            editorState.AssetOperationFeedback.Set(
                Editor::EditorFeedbackSeverity::Success,
                "Asset imported",
                "Asset import succeeded. Project assets will be reloaded."
            );
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (EditorButton("Cancel", SZ::Px(ImVec2(125.0f, 0.0f))))
        {
            errorMessage.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

}