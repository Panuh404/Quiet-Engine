#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <imgui.h>

#include "Renderer/UI/Assets/ImGuiAssetBrowserDetails.h"
#include "Renderer/UI/Assets/ImGuiAssetBrowserUtils.h"
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
    }

    std::string RemapCurrentFolderAfterRename(const std::string& currentFolder, const std::string& previousFolder, const std::string& renamedFolder)
    {
        if (currentFolder == previousFolder) return renamedFolder;
        const std::string previousPrefix = previousFolder + "/";

        if (currentFolder.rfind(previousPrefix, 0) == 0)
        {
            return renamedFolder + currentFolder.substr(previousFolder.size());
        }
        return currentFolder;
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

    void DrawRemoveAssetConfirmationPopup(const Project::ProjectContext& projectContext, const std::string& assetID, bool& confirmed, bool& cancelled, const float uiScale)
    {
        confirmed = false;
        cancelled = false;

        if (assetID.empty()) return;

        const float modalWidth = 560.0f * uiScale;
        const float labelWidth = 92.0f * uiScale;

        const ImVec2 modalPadding = SZ::Px(ImVec2(15.0f, 15.0f));
        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool removeAssetOpen = true;

        // The Asset Browser body intentionally uses zero padding. Push explicit
        // modal padding before BeginPopupModal so the popup does not inherit it.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, modalPadding);
        const bool popupVisible = EditorBeginPopupModal("Remove Asset", removeAssetOpen, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleVar();

        if (!popupVisible) return;

        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
        if (!record)
        {
            ImGui::PushStyleColor( ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped("The selected asset is no longer registered.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float buttonWidth = SZ::Px(120.0f);
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            if (availableWidth > buttonWidth)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - buttonWidth);
            }

            if (EditorButton("OK", ImVec2(buttonWidth, 0.0f)))
            {
                cancelled = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
        ImGui::TextUnformatted("Remove asset from registry?");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
        ImGui::TextWrapped("This will unregister the asset from the current project. The source file on disk will not be deleted.");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
        ImGui::Separator();

        ImGui::PopStyleColor();
        ImGui::Spacing();

        const std::string typeLabel = std::string(Project::ToString(record->Type));
        DrawDetailRow("ID", record->ID.c_str(), labelWidth);
        DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
        DrawDetailRow("Source", record->BuiltIn ? "Built-in" : "Project", labelWidth);

        if (record->Path.empty())
        {
            DrawDetailRow("Path", "<none>", labelWidth);
        }
        else
        {
            DrawDetailRowWrapped("Path", record->Path, labelWidth);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColor::BorderSoft);
        ImGui::Separator();

        ImGui::PopStyleColor();
        ImGui::Spacing();

        const float cancelWidth = SZ::Px(130.0f);
        const float removeWidth = SZ::Px(160.0f);
        const float actionSpacing = ImGui::GetStyle().ItemSpacing.x;

        const float actionRowWidth = cancelWidth + actionSpacing + removeWidth;
        const float availableWidth = ImGui::GetContentRegionAvail().x;

        if (availableWidth > actionRowWidth)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - actionRowWidth);
        }

        if (EditorButton("Cancel", ImVec2(cancelWidth, 0.0f)))
        {
            cancelled = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        if (EditorButton("Remove Asset", ImVec2(removeWidth, 0.0f)))
        {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
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

    void DrawAssetFolderActionPopup(Project::ProjectContext& projectContext, Editor::EditorState& editorState, AssetBrowserState& state)
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
                    state.CurrentAssetFolder = createdFolder.generic_string();
                    state.SelectedContentFolder.clear();
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
                    state.CurrentAssetFolder = RemapCurrentFolderAfterRename(state.CurrentAssetFolder, previousFolder, renamedFolderString);
                    state.SelectedContentFolder = RemapCurrentFolderAfterRename(state.SelectedContentFolder, previousFolder, renamedFolderString);
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
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::Warning);
            ImGui::TextUnformatted("Delete empty folder?");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, EditorColor::TextMuted);
            ImGui::TextWrapped(
                "Only a completely empty physical folder can be deleted. "
                "Registered assets are never removed by this operation."
            );
            ImGui::PopStyleColor();

            ImGui::Spacing();

            DrawFolderPath("Folder", state.FolderOperationTarget);
            DrawFolderOperationError(state.FolderOperationErrorMessage);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float cancelWidth = SZ::Px(120.0f);
            const float deleteWidth = SZ::Px(150.0f);
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

            if (EditorButton("Delete Folder", ImVec2(deleteWidth, 0.0f)))
            {
                const std::string deletedFolder = state.FolderOperationTarget;
                const std::string parentFolder = GetParentAssetFolder(projectContext, deletedFolder);

                state.FolderOperationErrorMessage.clear();

                if (projectContext.DeleteEmptyAssetFolder(deletedFolder, state.FolderOperationErrorMessage))
                {
                    if (state.CurrentAssetFolder == deletedFolder)
                    {
                        state.CurrentAssetFolder = parentFolder;
                    }
                    if (state.SelectedContentFolder == deletedFolder)
                    {
                        state.SelectedContentFolder.clear();
                    }
                    editorState.AssetOperationFeedback.Set(Editor::EditorFeedbackSeverity::Success,
                        "Folder deleted",
                        "Deleted the empty folder '" + deletedFolder + "'." );
                    operationSucceeded = true;
                }
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
}
