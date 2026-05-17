#include "Renderer/UI/ImGuiLayer.h"

#include <imgui_internal.h>

#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorState.h"

namespace
{
    const char* GetSceneModeLabel(Quiet::Editor::SceneMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::SceneMode::Edit:     return "[EDIT]";
            case Quiet::Editor::SceneMode::Simulate: return "[SIMULATE]";
            case Quiet::Editor::SceneMode::Play:     return "[PLAY]";
            default:                                 return "[UNKNOWN]";
        }
    }

    bool DrawMenuAction(Quiet::Editor::EditorActions& actions,
                        Quiet::Editor::EditorActionID actionID)
    {
        const char* shortcut = actions.GetShortcut(actionID);
        if (shortcut[0] == '\0') shortcut = nullptr;

        const bool enabled = actions.CanExecute(actionID);
        const bool pressed = Quiet::Renderer::UI::EditorMenuItem(
            actions.GetLabel(actionID),
            shortcut,
            false,
            enabled
        );

        if (pressed) actions.Execute(actionID);
        return pressed;
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildMainMenuBar(Editor::EditorActions &editorActions, Editor::EditorState &editorState)
    {
        if (!ImGui::BeginMainMenuBar()) return;

        if (EditorBeginMenu("File"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::NewScene);
            DrawMenuAction(editorActions, Editor::EditorActionID::SaveScene);
            DrawMenuAction(editorActions, Editor::EditorActionID::LoadScene);
            ImGui::Separator();
            if (EditorMenuItem("Project Manager..."))
            {
                editorState.ProjectManager.RequestOpenPopup = true;
            }
            ImGui::EndMenu();
        }

        if (EditorBeginMenu("Edit"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::Undo);
            DrawMenuAction(editorActions, Editor::EditorActionID::Redo);
            ImGui::Separator();
            DrawMenuAction(editorActions, Editor::EditorActionID::DuplicateSelected);
            DrawMenuAction(editorActions, Editor::EditorActionID::DeleteSelected);
            ImGui::EndMenu();
        }

        if (EditorBeginMenu("Scene"))
        {
            DrawMenuAction(editorActions, Editor::EditorActionID::CreateEmptyActor);
            DrawMenuAction(editorActions, Editor::EditorActionID::FocusSelected);
            ImGui::Separator();
            DrawMenuAction(editorActions, Editor::EditorActionID::ToggleSimulate);
            DrawMenuAction(editorActions, Editor::EditorActionID::TogglePlay);
            ImGui::EndMenu();
        }

        if (EditorBeginMenu("View"))
        {
            if (EditorMenuItem("Reset Layout"))
            {
                m_RequestResetLayout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void ImGuiLayer::BuildTopbar(Editor::EditorActions& editorActions,
                                 Editor::EditorState& editorState)
    {
        const float topbarHeight = GetToolbarHeight();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, topbarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Scale(ImVec2(14.0f, 8.0f)));
        PushToolbarStyle();

        ImGui::Begin("##MainTopbar", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        if (m_Fonts.Bold) ImGui::PushFont(m_Fonts.Bold);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.0f, 1.0f));
        ImGui::TextUnformatted("QUIET");
        ImGui::PopStyleColor();
        if (m_Fonts.Bold) ImGui::PopFont();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarNew",
                Icons::New,
                "New",
                "New Scene"))
        {
            editorActions.Execute(Editor::EditorActionID::NewScene);
        }

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarSave",
                Icons::Save,
                "Save",
                "Save Scene (Ctrl+S)",
                false,
                editorActions.CanExecute(Editor::EditorActionID::SaveScene)))
        {
            editorActions.Execute(Editor::EditorActionID::SaveScene);
        }

        ImGui::SameLine();
        if (DrawToolbarButton(
                m_Fonts,
                "TopbarLoad",
                Icons::Load,
                "Load",
                "Load Scene",
                false,
                editorActions.CanExecute(Editor::EditorActionID::LoadScene)))
        {
            editorActions.Execute(Editor::EditorActionID::LoadScene);
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (editorState.Scene.Mode == Editor::SceneMode::Edit)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarSimulate",
                    Icons::Simulate,
                    "Sim",
                    "Simulate Runtime (F6)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::ToggleSimulate)))
            {
                editorActions.Execute(Editor::EditorActionID::ToggleSimulate);
            }

            ImGui::SameLine();

            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarPlay",
                    Icons::Play,
                    "Play",
                    "Play Game (F5)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::TogglePlay)))
            {
                editorActions.Execute(Editor::EditorActionID::TogglePlay);
            }
        }
        else if (editorState.Scene.Mode == Editor::SceneMode::Simulate)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarStopSimulate",
                    Icons::Stop,
                    "Stop",
                    "Stop Simulate (F6)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::ToggleSimulate),
                    ImVec2(
                        m_Fonts.HasIcons ? ToolbarStyle::ButtonSize() : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorActions.Execute(Editor::EditorActionID::ToggleSimulate);
            }
        }
        else if (editorState.Scene.Mode == Editor::SceneMode::Play)
        {
            if (DrawToolbarButton(
                    m_Fonts,
                    "TopbarStopPlay",
                    Icons::Stop,
                    "Stop",
                    "Stop Play (F5)",
                    false,
                    editorActions.CanExecute(Editor::EditorActionID::TogglePlay),
                    ImVec2(
                        m_Fonts.HasIcons ? ToolbarStyle::ButtonSize() : ToolbarStyle::WideButtonWidth(),
                        ToolbarStyle::ButtonHeight())))
            {
                editorActions.Execute(Editor::EditorActionID::TogglePlay);
            }
        }

        std::string statusText = GetSceneModeLabel(editorState.Scene.Mode);
        statusText += " ";
        statusText += editorState.Scene.DisplayName;

        if (editorState.Scene.Dirty && editorState.Scene.Mode == Editor::SceneMode::Edit)
        {
            statusText += " *";
        }

        const float rightTextWidth = ImGui::CalcTextSize(statusText.c_str()).x;
        const float availableX = ImGui::GetWindowContentRegionMax().x;
        const float cursorX = availableX - rightTextWidth;

        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        if (editorState.Scene.Mode == Editor::SceneMode::Play)
        {
            ImGui::TextColored(EditorColor::AccentHover, "%s", statusText.c_str());
        }
        else if (editorState.Scene.Mode == Editor::SceneMode::Simulate)
        {
            ImGui::TextColored(EditorColor::Success, "%s", statusText.c_str());
        }
        else if (editorState.Scene.Dirty)
        {
            ImGui::TextColored(EditorColor::Warning, "%s", statusText.c_str());
        }
        else
        {
            ImGui::TextColored(EditorColor::TextMuted, "%s", statusText.c_str());
        }

        PopToolbarStyle();
        ImGui::End();
    }
}