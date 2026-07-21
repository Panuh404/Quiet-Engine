#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <string>
#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserOperations.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    namespace Assets = Quiet::Renderer::UI::Assets;


    ImVec4 MakeAssetFeedbackTextColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success:
                return QUI::EditorColor::Success;

            case Quiet::Editor::EditorFeedbackSeverity::Warning:
                return QUI::EditorColor::Warning;

            case Quiet::Editor::EditorFeedbackSeverity::Error:
                return QUI::EditorColor::Error;

            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default:
                return QUI::EditorColor::Accent;
        }
    }

    ImVec4 MakeAssetFeedbackBackgroundColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success:
                return ImVec4(0.035f, 0.105f, 0.065f, 1.0f);

            case Quiet::Editor::EditorFeedbackSeverity::Warning:
                return ImVec4(0.130f, 0.095f, 0.030f, 1.0f);

            case Quiet::Editor::EditorFeedbackSeverity::Error:
                return ImVec4(0.140f, 0.045f, 0.050f, 1.0f);

            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default:
                return ImVec4(0.040f, 0.075f, 0.135f, 1.0f);
        }
    }

    std::string TrimCopy(std::string value)
    {
        const auto isSpace = [](const unsigned char character)
        {
            return std::isspace(character) != 0;
        };
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return value;
    }

    void ClearFolderAction(Assets::AssetBrowserState& state)
    {
        state.PendingFolderAction = Assets::AssetBrowserFolderAction::None;
        state.FolderOperationTarget.clear();
        state.FolderOperationErrorMessage.clear();
        state.FolderNameBuffer.fill('\0');
        state.ConfirmPermanentFolderDelete = false;
    }

    void ClearAssetRename(Assets::AssetBrowserState& state)
    {
        state.RenameAssetID.clear();
        state.AssetRenameErrorMessage.clear();
        state.AssetRenameBuffer.fill('\0');
        state.RequestOpenAssetRenamePopup = false;
    }

    void ClearAssetRegistryRemoval(Assets::AssetBrowserState& state)
    {
        state.ConfirmRemoveAssetIDs.clear();
        state.RemoveErrorMessage.clear();
        state.RequestOpenRemovePopup = false;
    }

    void ClearAssetProjectDeletion(Assets::AssetBrowserState& state)
    {
        state.ConfirmDeleteAssetIDs.clear();
        state.DeleteErrorMessage.clear();
        state.RequestOpenDeletePopup = false;
        state.ConfirmPermanentAssetDelete = false;
    }

    void DrawFolderPath(const char* label, const std::string& folderPath)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextWrapped("%s", folderPath.c_str());
        ImGui::PopStyleColor();
    }

    void DrawFolderOperationError(const std::string& errorMessage)
    {
        if (errorMessage.empty()) return;
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);
        ImGui::TextWrapped("%s", errorMessage.c_str());
        ImGui::PopStyleColor();
    }

    void AlignFolderActionButtons(const float totalWidth)
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth > totalWidth)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - totalWidth);
        }
    }
}

namespace Quiet::Renderer::UI::Assets
{
    void DrawFailurePopup(const char* popupTitle, std::string& errorMessage)
    {
        if (errorMessage.empty()) return;
        bool popupOpen = true;

        if (!EditorBeginPopupModal(popupTitle, popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::PushTextWrapPos(SZ::Px(460.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Error);
        ImGui::TextWrapped("%s", errorMessage.c_str());
        ImGui::PopStyleColor();

        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
        {
            errorMessage.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void DrawAssetOperationFeedback(Editor::EditorState& editorState)
    {
        auto& feedback = editorState.AssetOperationFeedback;
        if (!feedback.Visible) return;

        const ImVec4 accentColor = MakeAssetFeedbackTextColor(feedback.Severity);
        const ImVec4 backgroundColor = MakeAssetFeedbackBackgroundColor(feedback.Severity);

        const std::string title = feedback.Title.empty() ? "Asset operation" : feedback.Title;
        const std::string message = feedback.Message.empty() ? "Operation completed." : feedback.Message;

        const ImVec2 feedbackPadding = SZ::Px(ImVec2(12.0f, 10.0f));

        const float availableWidth = std::max(SZ::Px(180.0f), ImGui::GetContentRegionAvail().x);
        const float messageWrapWidth = std::max(SZ::Px(1.0f), availableWidth - feedbackPadding.x * 2.0f - SZ::Px(4.0f));
        const float titleRowHeight = std::max(ImGui::GetTextLineHeight(), ImGui::GetFrameHeight());

        const float messageHeight = ImGui::CalcTextSize(
            message.c_str(),
            nullptr,
            false,
            messageWrapWidth
        ).y;

        const float feedbackHeight = std::max(
            SZ::Px(74.0f),
            feedbackPadding.y * 2.0f +
            titleRowHeight +
            ImGui::GetStyle().ItemSpacing.y +
            messageHeight
        );

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, feedbackPadding);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Border, accentColor);

        ImGui::BeginChild("##AssetOperationFeedback", ImVec2(0.0f, feedbackHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            const float dismissWidth = SZ::Px(82.0f);
            const float dismissX = ImGui::GetWindowContentRegionMax().x - dismissWidth;

            ImGui::SameLine();

            if (dismissX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(dismissX);
            if (EditorButton("Dismiss", ImVec2(dismissWidth, 0.0f)))
                feedback.Clear();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    const char* GetAssetFolderActionPopupTitle(const AssetBrowserFolderAction action)
    {
        switch (action)
        {
            case AssetBrowserFolderAction::Create:  return "Create Folder";
            case AssetBrowserFolderAction::Rename:  return "Rename Folder";
            case AssetBrowserFolderAction::Delete:  return "Delete Folder";
            case AssetBrowserFolderAction::None:
            default:                                return nullptr;
        }
    }

    void BeginAssetFolderAction(AssetBrowserState& state, const AssetBrowserFolderActionRequest& request)
    {
        state.PendingFolderAction = request.Action;
        state.FolderOperationTarget = request.TargetFolder;
        state.FolderOperationErrorMessage.clear();
        state.FolderNameBuffer.fill('\0');
        state.ConfirmPermanentFolderDelete = false;

        switch (request.Action)
        {
            case AssetBrowserFolderAction::Create:
                CopyToBuffer(state.FolderNameBuffer, "New Folder");
                break;

            case AssetBrowserFolderAction::Rename:
                CopyToBuffer(state.FolderNameBuffer, std::filesystem::path(request.TargetFolder).filename().string());
                break;

            case AssetBrowserFolderAction::Delete:
            case AssetBrowserFolderAction::None:
            default:
                break;
        }
    }

    void DrawAssetFolderActionPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        AssetBrowserState& state)
    {
        const AssetBrowserFolderAction action = state.PendingFolderAction;
        if (action == AssetBrowserFolderAction::None) return;

        const char* popupTitle = GetAssetFolderActionPopupTitle(action);
        if (!popupTitle) return;

        ImGui::SetNextWindowSize(SZ::Px(ImVec2(470.0f, 0.0f)), ImGuiCond_Appearing);

        bool popupOpen = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(18.0f, 16.0f)));
        const bool popupVisible = EditorBeginPopupModal(popupTitle, popupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        if (!popupVisible)
        {
            if (!popupOpen) ClearFolderAction(state);
            return;
        }

        bool operationSucceeded = false;
        if (action == AssetBrowserFolderAction::Create)
        {
            DrawFolderPath("Create inside", state.FolderOperationTarget);

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextUnformatted("Folder Name");
            ImGui::PopStyleColor();

            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::SetNextItemWidth(-1.0f);

            const bool submittedByEnter =
                ImGui::InputText("##CreateAssetFolderName", state.FolderNameBuffer.data(), state.FolderNameBuffer.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            DrawFolderOperationError(state.FolderOperationErrorMessage);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const std::string folderName = TrimCopy(state.FolderNameBuffer.data());
            const bool canCreate = !folderName.empty();
            const float buttonWidth = SZ::Px(120.0f);
            const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;

            AlignFolderActionButtons(totalWidth);

            if (EditorButton("Cancel", ImVec2(buttonWidth, 0.0f)))
            {
                ClearFolderAction(state);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (EditorButton("Create", ImVec2(buttonWidth, 0.0f), canCreate) || (submittedByEnter && canCreate))
            {
                std::filesystem::path createdFolder;
                state.FolderOperationErrorMessage.clear();

                if (projectContext.CreateAssetFolder(state.FolderOperationTarget, folderName, createdFolder, state.FolderOperationErrorMessage))
                {
                    NavigateToAssetFolder(projectContext, state, createdFolder.generic_string());
                    editorState.AssetOperationFeedback.Set(
                        Editor::EditorFeedbackSeverity::Success,
                        "Folder created",
                        "Created '" +
                        state.CurrentAssetFolder +
                        "'."
                    );
                    operationSucceeded = true;
                }
            }
        }
        else if (action == AssetBrowserFolderAction::Rename)
        {
            DrawFolderPath("Rename folder", state.FolderOperationTarget);

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextUnformatted("New Name");
            ImGui::PopStyleColor();

            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1.0f);

            const bool submittedByEnter =
                ImGui::InputText(
                    "##RenameAssetFolderName",
                    state.FolderNameBuffer.data(),
                    state.FolderNameBuffer.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_AutoSelectAll
                );

            DrawFolderOperationError(state.FolderOperationErrorMessage);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const std::string newName = TrimCopy(state.FolderNameBuffer.data());
            const bool canRename = !newName.empty();
            const float buttonWidth = SZ::Px(120.0f);
            const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;

            AlignFolderActionButtons(totalWidth);

            if (EditorButton("Cancel", ImVec2(buttonWidth, 0.0f)))
            {
                ClearFolderAction(state);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (EditorButton("Rename", ImVec2(buttonWidth, 0.0f), canRename) || (submittedByEnter && canRename))
            {
                const std::string previousFolder = state.FolderOperationTarget;
                std::filesystem::path renamedFolder;
                state.FolderOperationErrorMessage.clear();

                if (projectContext.RenameAssetFolder(previousFolder, newName, renamedFolder, state.FolderOperationErrorMessage))
                {
                    const std::string renamedFolderString = renamedFolder.generic_string();
                    RemapAssetFolderHistoryAfterRename(state, previousFolder, renamedFolderString);
                    editorState.QueueRequest(Editor::EditorRequestType::ReloadAssets);
                    editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
                        "Folder renamed",
                        "Renamed '" + previousFolder + "' to '" + renamedFolderString + "'. Registered asset paths were updated.");
                    operationSucceeded = true;
                }
            }
        }
        else if (action == AssetBrowserFolderAction::Delete)
        {
            const AssetFolderDeleteSummary summary = InspectAssetFolderForDeletion(projectContext, scene, state.FolderOperationTarget);

            const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
            const bool selectionCanBeDeleted = editMode && summary.CanDelete();
            const bool canDelete = selectionCanBeDeleted && state.ConfirmPermanentFolderDelete;

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextUnformatted("Permanently delete this folder?");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped(
                "This recursively deletes the physical folder, every "
                "nested file and subfolder, and all registry records "
                "whose paths belong to it.");

            ImGui::Spacing();

            ImGui::TextWrapped(
                "Unregistered files are also deleted. This operation "
                "cannot be undone. References in the active scene are "
                "validated, but other saved scenes are not scanned.");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            DrawFolderPath("Folder", state.FolderOperationTarget);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float labelWidth = SZ::Px(152.0f);

            DrawDetailRowUInt("Registered Assets", static_cast<std::uint32_t>(summary.RegisteredAssetIDs.size()), labelWidth);
            DrawDetailRowUInt("Physical Files", summary.PhysicalFileCount, labelWidth);
            DrawDetailRowUInt("Unregistered Files", summary.UnregisteredFileCount, labelWidth);
            DrawDetailRowUInt("Nested Folders", summary.NestedFolderCount, labelWidth);
            DrawDetailRowUInt("Missing Sources", summary.MissingRegisteredAssetCount, labelWidth);
            DrawDetailRowUInt("Assets In Use", summary.ReferencedAssetCount, labelWidth);
            DrawDetailRowUInt("Scene References", summary.SceneReferenceCount, labelWidth);
            DrawDetailRowUInt("Symbolic Links", summary.SymbolicLinkCount, labelWidth);
            DrawDetailRowUInt("Unsupported Entries", summary.UnsupportedEntryCount, labelWidth);

            if (!summary.RegisteredAssetIDs.empty())
            {
                ImGui::Spacing();

                ImGui::BeginChild("##DeleteFolderAssetList", ImVec2(0.0f, SZ::Px(170.0f)), ImGuiChildFlags_Borders);
                {
                    for (const std::string& assetID : summary.RegisteredAssetIDs)
                    {
                        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
                        const std::uint32_t referenceCount = record ? CountAssetReferences(scene, *record) : 0;

                        const bool sourceMissing = record && InspectAssetPath(projectContext, *record).IsMissing();
                        ImVec4 statusColor = EditorColor::Success;
                        std::string status = "Ready";

                        if (referenceCount > 0)
                        {
                            status = "In use by " + std::to_string(referenceCount) + (referenceCount == 1 ? " actor" : " actors");
                            statusColor = EditorColor::Warning;
                        }
                        else if (sourceMissing)
                        {
                            status = "Stale registry record";
                            statusColor = EditorColor::Warning;
                        }

                        ImGui::BulletText("%s", assetID.c_str());

                        ImGui::SameLine();

                        ImGui::TextColored(statusColor, "— %s", status.c_str());
                    }
                }
                ImGui::EndChild();
            }

            if (!summary.ErrorMessage.empty())
            {
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
                ImGui::TextWrapped("%s", summary.ErrorMessage.c_str());
                ImGui::PopStyleColor();
            }

            DrawFolderOperationError(state.FolderOperationErrorMessage);

            if (!selectionCanBeDeleted)
            {
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
                ImGui::TextWrapped(editMode
                    ? "The folder must be fully inspectable, contain no active-scene references, symbolic links, or unsupported filesystem entries."
                    : "Folders cannot be deleted while runtime mode is active.");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            ImGui::Checkbox("I understand that this folder and all of its contents will be permanently deleted.", &state.ConfirmPermanentFolderDelete);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float cancelWidth = SZ::Px(120.0f);
            const float deleteWidth = SZ::Px(190.0f);
            const float totalWidth = cancelWidth + deleteWidth + ImGui::GetStyle().ItemSpacing.x;

            AlignFolderActionButtons(totalWidth);

            if (EditorButton("Cancel", ImVec2(cancelWidth, 0.0f)))
            {
                ClearFolderAction(state);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

            if (EditorButton("Delete Permanently", ImVec2(deleteWidth, 0.0f), canDelete))
            {
                state.FolderOperationErrorMessage.clear();
                operationSucceeded = ExecuteAssetFolderDeleteOperation(projectContext, editorState, state, scene, state.FolderOperationTarget);
            }

            ImGui::PopStyleColor(3);
        }

        if (operationSucceeded)
        {
            ClearFolderAction(state);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Rename
    void RequestAssetRename(AssetBrowserState &state, const Project::AssetRecord &record)
    {
        if (record.ID.empty() || record.BuiltIn || record.Path.empty()) return;

        const std::filesystem::path sourcePath = record.Path;
        std::string currentBaseName = sourcePath.stem().string();

        if (currentBaseName.empty()) currentBaseName = record.ID;
        state.RenameAssetID = record.ID;
        state.AssetRenameErrorMessage.clear();

        CopyToBuffer(state.AssetRenameBuffer, currentBaseName);

        state.RequestOpenAssetRenamePopup = true;
    }

    void DrawAssetRenamePopup(Project::ProjectContext &projectContext, Editor::EditorState &editorState, AssetBrowserState &state)
    {
        if (state.RequestOpenAssetRenamePopup)
        {
            ImGui::OpenPopup("Rename Asset");
            state.RequestOpenAssetRenamePopup = false;
        }

        if (state.RenameAssetID.empty()) return;
        ImGui::SetNextWindowSize(SZ::Px(ImVec2(500.0f, 0.0f)), ImGuiCond_Appearing);
        bool popupOpen = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(18.0f, 16.0f)));
        const bool popupVisible =
            EditorBeginPopupModal("Rename Asset", popupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        if (!popupVisible)
        {
            if (!popupOpen) ClearAssetRename(state);
            return;
        }

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), state.RenameAssetID);
        if (!record)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("The selected asset is no longer registered.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (EditorButton("Close", SZ::Px(ImVec2(120.0f, 0.0f))))
            {
                ClearAssetRename(state);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
            return;
        }

        const std::filesystem::path currentPath = record->Path;
        const std::string extension = currentPath.extension().string();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted("Stable Asset ID");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextWrapped("%s", record->ID.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted("Current File");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Text);
        ImGui::TextWrapped("%s", currentPath.filename().string().c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextUnformatted("New File Name");
        ImGui::PopStyleColor();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(-1.0f);

        const bool submittedByEnter =
            ImGui::InputText("##RenameAssetFileName", state.AssetRenameBuffer.data(), state.AssetRenameBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (!extension.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::Text("The existing extension %s will be preserved.", extension.c_str());
            ImGui::PopStyleColor();
        }

        if (!state.AssetRenameErrorMessage.empty())
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("%s", state.AssetRenameErrorMessage.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("The stable Asset ID will not change, so existing scene and Inspector references remain valid.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const std::string newName =  TrimCopy(state.AssetRenameBuffer.data());
        const bool canRename = !newName.empty();
        const float buttonWidth = SZ::Px(120.0f);
        const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;

        AlignFolderActionButtons(totalWidth);

        if (EditorButton("Cancel", ImVec2(buttonWidth, 0.0f)))
        {
            ClearAssetRename(state);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (EditorButton("Rename", ImVec2(buttonWidth, 0.0f), canRename) || (submittedByEnter && canRename))
        {
            AssetBrowserOperationRequest request{};
            request.Action = AssetBrowserOperationAction::Rename;
            request.AssetIDs = { state.RenameAssetID };
            request.NewName = newName;

            if (ExecuteAssetBrowserOperation(projectContext, editorState, state, request))
            {
                ClearAssetRename(state);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    // Remove
    void RequestAssetRegistryRemoval(AssetBrowserState& state, const std::vector<std::string>& assetIDs)
    {
        state.ConfirmRemoveAssetIDs.clear();

        for (const std::string& assetID : assetIDs)
        {
            if (assetID.empty()) continue;
            if (std::find(
                state.ConfirmRemoveAssetIDs.begin(),
                state.ConfirmRemoveAssetIDs.end(),
                assetID) == state.ConfirmRemoveAssetIDs.end())
            {
                state.ConfirmRemoveAssetIDs.push_back(assetID );
            }
        }

        if (state.ConfirmRemoveAssetIDs.empty()) return;
        state.RemoveErrorMessage.clear();
        state.RequestOpenRemovePopup = true;
    }

    void DrawRemoveAssetConfirmationPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        AssetBrowserState& state,
        const float uiScale)
    {
        if (state.RequestOpenRemovePopup)
        {
            ImGui::OpenPopup("Remove Assets from Registry");
            state.RequestOpenRemovePopup = false;
        }
        if (state.ConfirmRemoveAssetIDs.empty()) return;

        ImGui::SetNextWindowSize(ImVec2(620.0f * uiScale, 0.0f), ImGuiCond_Appearing);
        bool popupOpen = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(16.0f, 16.0f)));
        const bool popupVisible = EditorBeginPopupModal(
                "Remove Assets from Registry",
                popupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        if (!popupVisible)
        {
            if (!popupOpen) ClearAssetRegistryRemoval(state);
            return;
        }

        std::uint32_t removableCount = 0;
        std::uint32_t builtInCount = 0;
        std::uint32_t referencedCount = 0;
        std::uint32_t missingCount = 0;

        for (const std::string& assetID :
             state.ConfirmRemoveAssetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                ++missingCount;
                continue;
            }
            if (record->BuiltIn)
            {
                ++builtInCount;
                continue;
            }
            if (CountAssetReferences(scene, *record) > 0)
            {
                ++referencedCount;
                continue;
            }
            ++removableCount;
        }

        const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
        const bool canRemove = editMode && missingCount == 0 && builtInCount == 0 && referencedCount == 0 && removableCount == state.ConfirmRemoveAssetIDs.size();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
        ImGui::Text(state.ConfirmRemoveAssetIDs.size() == 1
                ? "Remove asset from registry?"
                : "Remove %zu assets from registry?", state.ConfirmRemoveAssetIDs.size() );
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("This removes the selected registry records from the current project. Physical source files will remain on disk.");

        ImGui::Spacing();

        ImGui::TextWrapped("References in the active scene are validated. Other scene files are not scanned by this operation.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float labelWidth = SZ::Px(118.0f);

        DrawDetailRowUInt("Selected", static_cast<std::uint32_t>(state.ConfirmRemoveAssetIDs.size()), labelWidth);
        DrawDetailRowUInt("Removable", removableCount, labelWidth);
        DrawDetailRowUInt("Built In", builtInCount, labelWidth);
        DrawDetailRowUInt("In Use", referencedCount, labelWidth);
        DrawDetailRowUInt("Missing", missingCount, labelWidth);

        ImGui::Spacing();

        ImGui::BeginChild("##RemoveRegistryAssetList", ImVec2(0.0f, SZ::Px(180.0f)), ImGuiChildFlags_Borders);
        {
            for (const std::string& assetID : state.ConfirmRemoveAssetIDs)
            {
                const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
                std::string status;
                ImVec4 statusColor = EditorColor::Success;
                if (!record)
                {
                    status = "No longer registered";
                    statusColor = EditorColor::Error;
                }
                else if (record->BuiltIn)
                {
                    status = "Built-in";
                    statusColor = EditorColor::Error;
                }
                else
                {
                    const std::uint32_t referenceCount = CountAssetReferences(scene, *record);
                    if (referenceCount > 0)
                    {
                        status = "In use by " + std::to_string(referenceCount) + (referenceCount == 1 ? " actor" : " actors");
                        statusColor = EditorColor::Warning;
                    }
                    else
                    {
                        status = "Ready";
                    }
                }

                ImGui::BulletText("%s", assetID.c_str());
                ImGui::SameLine();
                ImGui::TextColored(statusColor, "— %s", status.c_str());
            }
        }
        ImGui::EndChild();

        if (!state.RemoveErrorMessage.empty())
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("%s", state.RemoveErrorMessage.c_str());
            ImGui::PopStyleColor();
        }

        if (!canRemove)
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextWrapped(editMode
                ? "Every selected asset must be removable before the batch can continue."
                : "Assets cannot be removed while runtime mode is active.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float cancelWidth = SZ::Px(130.0f);
        const float removeWidth = SZ::Px(190.0f);
        const float totalWidth = cancelWidth + removeWidth + ImGui::GetStyle().ItemSpacing.x;

        AlignFolderActionButtons(totalWidth);

        if (EditorButton("Cancel", ImVec2(cancelWidth, 0.0f)))
        {
            ClearAssetRegistryRemoval(state);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        const char* removeLabel = state.ConfirmRemoveAssetIDs.size() == 1 ? "Remove from Registry" : "Remove Selection";

        if (EditorButton(removeLabel, ImVec2(removeWidth, 0.0f), canRemove))
        {
            AssetBrowserOperationRequest request{};
            request.Action = AssetBrowserOperationAction::RemoveFromRegistry;
            request.AssetIDs = state.ConfirmRemoveAssetIDs;

            if (ExecuteAssetBrowserOperation(projectContext, editorState, state, request, &scene))
            {
                ClearAssetRegistryRemoval(state);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
    }

    // Delete
    void RequestAssetProjectDeletion(AssetBrowserState& state, const std::vector<std::string>& assetIDs)
    {
        state.ConfirmDeleteAssetIDs.clear();

        for (const std::string& assetID : assetIDs)
        {
            if (assetID.empty()) continue;
            if (std::find(state.ConfirmDeleteAssetIDs.begin(), state.ConfirmDeleteAssetIDs.end(), assetID) == state.ConfirmDeleteAssetIDs.end())
            {
                state.ConfirmDeleteAssetIDs.push_back(assetID);
            }
        }

        if (state.ConfirmDeleteAssetIDs.empty()) return;

        state.DeleteErrorMessage.clear();
        state.ConfirmPermanentAssetDelete = false;
        state.RequestOpenDeletePopup = true;
    }

    void DrawDeleteAssetConfirmationPopup(
        Project::ProjectContext& projectContext,
        Editor::EditorState& editorState,
        const Scene::Scene& scene,
        AssetBrowserState& state,
        const float uiScale)
    {
        if (state.RequestOpenDeletePopup)
        {
            ImGui::OpenPopup("Delete Assets from Project");
            state.RequestOpenDeletePopup = false;
        }

        if (state.ConfirmDeleteAssetIDs.empty()) return;
        ImGui::SetNextWindowSize(ImVec2(660.0f * uiScale, 0.0f), ImGuiCond_Appearing);

        bool popupOpen = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(16.0f, 16.0f)));
        const bool popupVisible = EditorBeginPopupModal("Delete Assets from Project", popupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        if (!popupVisible)
        {
            if (!popupOpen) ClearAssetProjectDeletion(state);
            return;
        }

        std::uint32_t deletableCount = 0;
        std::uint32_t builtInCount = 0;
        std::uint32_t referencedCount = 0;
        std::uint32_t missingSourceCount = 0;
        std::uint32_t externalCount = 0;
        std::uint32_t missingRegistryCount = 0;

        for (const std::string& assetID : state.ConfirmDeleteAssetIDs)
        {
            const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
            if (!record)
            {
                ++missingRegistryCount;
                continue;
            }
            if (record->BuiltIn)
            {
                ++builtInCount;
                continue;
            }

            const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
            if (diagnostics.IsMissing())
            {
                ++missingSourceCount;
                continue;
            }
            if (diagnostics.External)
            {
                ++externalCount;
                continue;
            }

            if (CountAssetReferences(scene, *record) > 0)
            {
                ++referencedCount;
                continue;
            }

            ++deletableCount;
        }

        const bool editMode = editorState.Scene.Mode == Editor::SceneMode::Edit;

        const bool selectionIsDeletable =
            editMode &&
            builtInCount == 0 &&
            referencedCount == 0 &&
            missingSourceCount == 0 &&
            externalCount == 0 &&
            missingRegistryCount == 0 &&
            deletableCount == state.ConfirmDeleteAssetIDs.size();

        const bool canDelete = selectionIsDeletable && state.ConfirmPermanentAssetDelete;

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
        ImGui::Text(state.ConfirmDeleteAssetIDs.size() == 1
            ? "Permanently delete this asset?"
            : "Permanently delete %zu assets?",
            state.ConfirmDeleteAssetIDs.size());
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("This permanently deletes the physical source files from the project Assets directory and removes their registry records.");

        ImGui::Spacing();

        ImGui::TextWrapped("This operation cannot be undone. References in the active scene are validated, but other saved scene files are not scanned.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float labelWidth = SZ::Px(126.0f);

        DrawDetailRowUInt("Selected", static_cast<std::uint32_t>(state.ConfirmDeleteAssetIDs.size()), labelWidth);
        DrawDetailRowUInt("Deletable", deletableCount, labelWidth);
        DrawDetailRowUInt("Built In", builtInCount, labelWidth);
        DrawDetailRowUInt("In Use", referencedCount, labelWidth);
        DrawDetailRowUInt("Missing Source", missingSourceCount, labelWidth);
        DrawDetailRowUInt("External", externalCount, labelWidth);
        DrawDetailRowUInt("Not Registered", missingRegistryCount, labelWidth);

        ImGui::Spacing();

        ImGui::BeginChild("##DeleteProjectAssetList", ImVec2(0.0f, SZ::Px(190.0f)), ImGuiChildFlags_Borders);
        {
            for (const std::string& assetID : state.ConfirmDeleteAssetIDs)
            {
                const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);

                std::string status;
                ImVec4 statusColor = EditorColor::Success;

                if (!record)
                {
                    status = "No longer registered";
                    statusColor = EditorColor::Error;
                }
                else if (record->BuiltIn)
                {
                    status = "Built-in";
                    statusColor = EditorColor::Error;
                }
                else
                {
                    const AssetPathDiagnostics diagnostics = InspectAssetPath(projectContext, *record);
                    if (diagnostics.IsMissing())
                    {
                        status = MakeAssetPathDiagnosticLabel(diagnostics);
                        statusColor = EditorColor::Error;
                    }
                    else if (diagnostics.External)
                    {
                        status = "External";
                        statusColor = EditorColor::Error;
                    }
                    else
                    {
                        const std::uint32_t referenceCount = CountAssetReferences(scene, *record);
                        if (referenceCount > 0)
                        {
                            status = "In use by " + std::to_string(referenceCount) + (referenceCount == 1 ? " actor" : " actors");
                            statusColor = EditorColor::Warning;
                        }
                        else
                        {
                            status = "Ready";
                        }
                    }
                }

                ImGui::BulletText("%s", assetID.c_str());

                ImGui::SameLine();

                ImGui::TextColored(statusColor, "— %s", status.c_str());
            }
        }
        ImGui::EndChild();

        if (!state.DeleteErrorMessage.empty())
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Error);
            ImGui::TextWrapped("%s", state.DeleteErrorMessage.c_str());
            ImGui::PopStyleColor();
        }

        if (!selectionIsDeletable)
        {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextWrapped(editMode
                ? "Every selected asset must have an unreferenced physical source file inside Project/Assets."
                : "Assets cannot be deleted while runtime mode is active.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        ImGui::Checkbox("I understand that these files will be permanently deleted.", &state.ConfirmPermanentAssetDelete);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float cancelWidth = SZ::Px(130.0f);
        const float deleteWidth = SZ::Px(190.0f);
        const float totalWidth = cancelWidth + deleteWidth + ImGui::GetStyle().ItemSpacing.x;

        AlignFolderActionButtons(totalWidth);

        if (EditorButton("Cancel", ImVec2(cancelWidth, 0.0f)))
        {
            ClearAssetProjectDeletion(state);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        if (EditorButton("Delete Permanently", ImVec2(deleteWidth, 0.0f), canDelete))
        {
            AssetBrowserOperationRequest request{};
            request.Action = AssetBrowserOperationAction::DeleteFromProject;
            request.AssetIDs = state.ConfirmDeleteAssetIDs;

            if (ExecuteAssetBrowserOperation(projectContext, editorState, state, request, &scene))
            {
                ClearAssetProjectDeletion(state);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }
}
