#include "Renderer/UI/Assets/ImGuiAssetBrowserImport.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <imgui.h>

#include "Quiet/Project/AssetRegistry.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    std::filesystem::path ResolveImportSourcePath(const Quiet::Project::ProjectContext& projectContext, const std::string& sourcePath)
    {
        std::filesystem::path resolvedPath = sourcePath;
        if (resolvedPath.is_relative()) resolvedPath = projectContext.GetRootPath() / resolvedPath;
        return resolvedPath.lexically_normal();
    }

    bool PathsReferToSameFile(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        if (left.lexically_normal() == right.lexically_normal()) return true;

        std::error_code ec;
        if (!std::filesystem::exists(left, ec) || ec) return false;

        ec.clear();
        if (!std::filesystem::exists(right, ec) || ec) return false;

        ec.clear();
        return std::filesystem::equivalent(left, right, ec) && !ec;
    }

    const Quiet::Project::AssetRecord* FindRegisteredAssetAtPath(const Quiet::Project::ProjectContext& projectContext, const std::filesystem::path& assetPath)
    {
        for (const Quiet::Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (!record.IsValid() || record.BuiltIn || record.Path.empty()) continue;
            const std::filesystem::path registeredPath = Quiet::Renderer::UI::Assets::ResolveDisplayPath(projectContext, record.Path);
            if (PathsReferToSameFile(registeredPath, assetPath)) return &record;
        }
        return nullptr;
    }

    bool ImportAssetIntoSelectedFolder(
        Quiet::Project::ProjectContext& projectContext,
        Quiet::Editor::EditorState& editorState,
        const std::string& currentAssetFolder,
        const std::string& assetID,
        const std::string& sourcePathText,
        const Quiet::Project::AssetType assetType,
        std::string& errorMessage)
    {
        namespace Assets = Quiet::Renderer::UI::Assets;
        namespace Project = Quiet::Project;

        if (assetID.empty())
        {
            errorMessage = "Asset ID cannot be empty.";
            return false;
        }

        if (Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID))
        {
            errorMessage = "An asset with ID '" + assetID + "' already exists. Use Reimport for an existing asset.";
            return false;
        }

        const std::filesystem::path sourcePath = ResolveImportSourcePath(projectContext, sourcePathText);

        std::error_code ec;
        if (!std::filesystem::exists(sourcePath, ec) || ec)
        {
            errorMessage = "The selected source file does not exist.";
            return false;
        }

        ec.clear();
        if (!std::filesystem::is_regular_file(sourcePath, ec) || ec)
        {
            errorMessage = "The selected source path is not a regular file.";
            return false;
        }

        const std::string assetsRoot = Assets::GetAssetsRootRelativePath(projectContext);
        const std::string requestedFolder = currentAssetFolder.empty()
                ? assetsRoot
                : Assets::NormalizeProjectRelativePathString(currentAssetFolder);

        std::string validatedFolder = requestedFolder;
        Assets::ValidateCurrentAssetFolder(projectContext, validatedFolder);
        if (validatedFolder != requestedFolder)
        {
            errorMessage = "The selected destination folder is no longer available.";
            return false;
        }

        const std::filesystem::path destinationFolderPath = Assets::ResolveAssetFolderPath(projectContext, validatedFolder);

        ec.clear();
        if (!std::filesystem::exists(destinationFolderPath, ec) || ec)
        {
            errorMessage = "The selected destination folder does not exist.";
            return false;
        }

        ec.clear();
        if (!std::filesystem::is_directory(destinationFolderPath, ec) || ec)
        {
            errorMessage = "The selected destination path is not a folder.";
            return false;
        }

        const std::filesystem::path sourceFilename = sourcePath.filename();
        if (sourceFilename.empty())
        {
            errorMessage = "The selected source file has no valid filename.";
            return false;
        }

        const std::filesystem::path destinationPath = (destinationFolderPath / sourceFilename).lexically_normal();
        const bool sourceAlreadyInDestination = PathsReferToSameFile(sourcePath, destinationPath);

        ec.clear();
        const bool destinationExists = std::filesystem::exists(destinationPath, ec);
        if (ec)
        {
            errorMessage = "Failed to inspect the destination file path.";
            return false;
        }

        if (destinationExists && !sourceAlreadyInDestination)
        {
            errorMessage = "A file named '" + sourceFilename.string() + "' already exists in the selected destination folder.";
            return false;
        }

        if (const Project::AssetRecord* existingRecord = FindRegisteredAssetAtPath(projectContext, destinationPath))
        {
            errorMessage = "The destination file is already registered as asset '" + existingRecord->ID + "'.";
            return false;
        }

        bool copiedFile = false;
        if (!sourceAlreadyInDestination)
        {
            ec.clear();
            copiedFile = std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, ec);
            if (!copiedFile || ec)
            {
                errorMessage = "Failed to copy the source file into the selected Assets folder";
                if (ec) errorMessage += ": " + ec.message();
                return false;
            }
        }

        bool registered = false;
        switch (assetType)
        {
            case Project::AssetType::Mesh:
                registered = projectContext.RegisterMeshAsset(assetID, destinationPath, false);
                break;

            case Project::AssetType::Texture:
                registered = projectContext.RegisterTextureAsset(assetID, destinationPath, false);
                break;

            case Project::AssetType::Unknown:
            default:
                registered = false;
                break;
        }

        if (!registered)
        {
            if (copiedFile)
            {
                ec.clear();
                std::filesystem::remove(destinationPath, ec);
            }
            errorMessage = "Failed to register the imported asset in the project registry.";
            return false;
        }

        editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
        return true;
    }
}
namespace Quiet::Renderer::UI::Assets
{
    bool DrawRegisterAssetForm(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const std::string& currentAssetFolder,
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 1024>& assetPathBuffer,
        std::string& errorMessage,
        bool& reopenImportPopupAfterFileDialog)
    {
        bool imported = false;

        const std::string currentPath = assetPathBuffer.data();
        const bool hasPath = !currentPath.empty();

        const std::filesystem::path resolvedSourcePath = hasPath
                ? ResolveImportSourcePath(projectContext, currentPath)
                : std::filesystem::path{};

        const bool fileExists = hasPath && DoesAssetPathExist(projectContext, currentPath);

        const Project::AssetType detectedType = hasPath
                ? DetectImportAssetType(resolvedSourcePath)
                : Project::AssetType::Unknown;

        const bool supportedType = detectedType != Project::AssetType::Unknown;

        const std::filesystem::path destinationFolderPath = ResolveAssetFolderPath(projectContext, currentAssetFolder);
        const std::filesystem::path destinationPath = hasPath && !resolvedSourcePath.filename().empty()
                ? (destinationFolderPath / resolvedSourcePath.filename()).lexically_normal()
                : std::filesystem::path{};

        const bool sourceAlreadyInDestination = fileExists && !destinationPath.empty() && PathsReferToSameFile(resolvedSourcePath, destinationPath);

        std::error_code destinationError;
        const bool destinationExists = !destinationPath.empty() && std::filesystem::exists(destinationPath, destinationError);
        const bool destinationConflict = !destinationError && destinationExists && !sourceAlreadyInDestination;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::TextUnformatted("Asset ID");
        ImGui::SetNextItemWidth(-FLT_MIN);

        ImGui::InputTextWithHint("##AssetID", "Leave empty to auto-generate from filename", assetIDBuffer.data(), assetIDBuffer.size());
        ImGui::TextUnformatted("Source File");

        const float browseWidth = SZ::Px(96.0f);
        ImGui::SetNextItemWidth(-(browseWidth + ImGui::GetStyle().ItemSpacing.x));

        ImGui::InputTextWithHint("##AssetPath", "Select a mesh or texture file", assetPathBuffer.data(), assetPathBuffer.size());
        ImGui::SameLine();

        if (EditorButton("Browse", ImVec2(browseWidth, 0.0f)))
        {
            OpenAssetFileDialog(projectContext, currentAssetFolder);
            reopenImportPopupAfterFileDialog = true;

            // Do not keep two modal popups stacked. The Import popup will be
            // restored after the file dialog has finished.
            ImGui::CloseCurrentPopup();
        }

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
        else if (!fileExists)
        {
            ImGui::TextColored(EditorColor::Error, "Source file not found");
        }
        else
        {
            const std::string typeLabel = std::string(Project::ToString(detectedType));
            ImGui::TextColored(EditorColor::Success, "%s source file found", typeLabel.c_str());
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Destination Folder");

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("%s", currentAssetFolder.c_str());

        ImGui::PopStyleColor();

        if (!destinationPath.empty())
        {
            const std::string destinationRegistryPath = projectContext.NormalizeAssetPath(destinationPath).generic_string();
            if (destinationError)
            {
                ImGui::TextColored(EditorColor::Error, "Unable to inspect the destination path.");
            }
            else if (destinationConflict)
            {
                ImGui::TextColored(EditorColor::Error, "A file named '%s' already exists in this folder.", resolvedSourcePath.filename().string().c_str());
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextDim);
                ImGui::TextWrapped(sourceAlreadyInDestination ? "Register in place: %s" : "Copy to: %s", destinationRegistryPath.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::Spacing();

        const bool canImport = hasPath && supportedType && fileExists && !destinationError && !destinationConflict;
        if (EditorButton("Import", SZ::Px(ImVec2(125.0f, 0.0f)), canImport))
        {
            std::string assetID = assetIDBuffer.data();
            if (assetID.empty()) assetID = ExtractFileNameNoExt(currentPath);
            errorMessage.clear();

            if (ImportAssetIntoSelectedFolder(projectContext, editorState, currentAssetFolder, assetID, currentPath, detectedType, errorMessage))
            {
                assetIDBuffer[0] = '\0';
                assetPathBuffer[0] = '\0';
                imported = true;
            }
            else
            {
                ImGui::OpenPopup("Import Asset Failed");
            }
        }

        ImGui::PopStyleVar(2);
        return imported;
    }

    void DrawImportAssetPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const std::string& currentAssetFolder,
        std::array<char, 128>& assetIDBuffer,
        std::array<char, 1024>& assetPathBuffer,
        std::string& errorMessage,
        bool& reopenImportPopupAfterFileDialog)
    {
        ImGui::SetNextWindowSize(SZ::Px(ImVec2(640.0f, 0.0f)), ImGuiCond_Appearing);

        bool importPopupOpen = true;
        if (!EditorBeginPopupModal("Import Asset", importPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            // This is a real titlebar-close action, not the temporary close
            // performed while Browse is active.
            if (!importPopupOpen && !reopenImportPopupAfterFileDialog)
            {
                assetIDBuffer.fill('\0');
                assetPathBuffer.fill('\0');
                errorMessage.clear();
            }
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextUnformatted("Import Asset");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped(
            "Choose a supported source file. Quiet will copy it "
            "into the selected project Assets folder and register "
            "its project-relative path."
        );
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        if (DrawRegisterAssetForm(projectContext, editorState, currentAssetFolder, assetIDBuffer, assetPathBuffer, errorMessage, reopenImportPopupAfterFileDialog))
        {
            editorState.AssetOperationFeedback.Set(
                Editor::EditorFeedbackSeverity::Success,
                "Asset imported",
                "The asset was copied into '" +
                currentAssetFolder +
                "' and registered. Project assets will be reloaded."
            );
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (EditorButton("Cancel", SZ::Px(ImVec2(125.0f, 0.0f))))
        {
            assetIDBuffer[0] = '\0';
            assetPathBuffer[0] = '\0';
            errorMessage.clear();
            reopenImportPopupAfterFileDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

}