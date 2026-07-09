#include "Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h"

#include <string>

#include <imgui.h>

#include "ImGuiAssetBrowserDetails.h"
#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

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

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Border, accentColor);

        ImGui::BeginChild("##AssetOperationFeedback", ImVec2(0.0f, SZ::Px(74.0f)),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            const std::string title = feedback.Title.empty() ? "Asset operation" : feedback.Title;
            const std::string message = feedback.Message.empty() ? "Operation completed." : feedback.Message;

            ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            const float dismissWidth = SZ::Px(82.0f);
            const float dismissX = ImGui::GetWindowContentRegionMax().x - dismissWidth;

            ImGui::SameLine();

            if (dismissX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(dismissX);

            if (EditorButton("Dismiss", ImVec2(dismissWidth, 0.0f)))
                feedback.Clear();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
    }

    void DrawRemoveAssetConfirmationPopup(
        const Project::ProjectContext &projectContext,
        const std::string &assetID,
        bool &confirmed,
        bool &cancelled,
        float uiScale)
    {
        confirmed = false;
        cancelled = false;

        if (assetID.empty()) return;

        const float modalWidth = 560.0f * uiScale;
        const float labelWidth = 92.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);
        bool removeAssetOpen = true;

        if (!EditorBeginPopupModal("Remove Asset", removeAssetOpen, ImGuiWindowFlags_AlwaysAutoResize)) return;
        const Project::AssetRecord* record = Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);

        if (!record)
        {
            ImGui::TextWrapped("The selected asset is no longer registered.");
            ImGui::Spacing();

            if (EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
            {
                cancelled = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Warning);
        ImGui::TextUnformatted("Remove asset from registry?");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::TextWrapped(
            "This will unregister the asset from the current project. "
            "The source file on disk will not be deleted."
        );

        ImGui::Spacing();
        ImGui::Separator();
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
        ImGui::Separator();
        ImGui::Spacing();

        if (EditorButton("Cancel", SZ::Px(ImVec2(130.0f, 0.0f))))
        {
            cancelled = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        if (EditorButton("Remove Asset", SZ::Px(ImVec2(160.0f, 0.0f))))
        {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
    }
}
