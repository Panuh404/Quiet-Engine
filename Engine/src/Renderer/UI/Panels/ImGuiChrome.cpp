#include "Renderer/UI/ImGuiLayer.h"

#include <string>
#include <imgui_internal.h>

#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorState.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;

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

    ImVec4 GetSceneModeColor(Quiet::Editor::SceneMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::SceneMode::Edit:        return QUI::EditorColor::TextMuted;
            case Quiet::Editor::SceneMode::Simulate:    return QUI::EditorColor::Success;
            case Quiet::Editor::SceneMode::Play:        return QUI::EditorColor::AccentHover;
            default:                                    return QUI::EditorColor::TextDim;
        }
    }

    void DrawTopbarSeparator()
    {
        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::PopStyleColor();
    }

    void DrawTopbarBrand(float uiScale)
    {
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float markerWidth = 3.0f * uiScale;
        const float markerHeight = ImGui::GetTextLineHeight() + 2.0f * uiScale;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            ImVec2(cursor.x, cursor.y + 1.0f * uiScale),
            ImVec2(cursor.x + markerWidth, cursor.y + markerHeight),
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentHover),
            2.0f * uiScale
        );

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * uiScale);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.0f, 1.0f));
        ImGui::TextUnformatted("QUIET");
        ImGui::PopStyleColor();
    }

    void DrawTopbarStatusPill(const Quiet::Editor::EditorState& editorState,
                              float uiScale)
    {
        std::string sceneName = editorState.Scene.DisplayName.empty()
            ? "Untitled Scene"
            : editorState.Scene.DisplayName;

        std::string statusText = GetSceneModeLabel(editorState.Scene.Mode);
        statusText += "  ";
        statusText += sceneName;

        if (editorState.Scene.Dirty && editorState.Scene.Mode == Quiet::Editor::SceneMode::Edit)
        {
            statusText += "  *";
        }

        const ImVec2 padding = ImVec2(10.0f * uiScale, 4.0f * uiScale);
        const ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());

        const ImVec2 pillSize(
            textSize.x + padding.x * 2.0f,
            textSize.y + padding.y * 2.0f
        );

        const float contentMaxX = ImGui::GetWindowContentRegionMax().x;
        const float currentX = ImGui::GetCursorPosX();
        const float targetX = contentMaxX - pillSize.x;

        if (targetX > currentX + 12.0f * uiScale)
            ImGui::SameLine(targetX);
        else
            ImGui::SameLine();

        const ImVec2 pillMin = ImGui::GetCursorScreenPos();
        const ImVec2 pillMax(
            pillMin.x + pillSize.x,
            pillMin.y + pillSize.y
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            pillMin,
            pillMax,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background2),
            8.0f * uiScale
        );
        drawList->AddRect(
            pillMin,
            pillMax,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            8.0f * uiScale
        );
        drawList->AddText(
            ImVec2(pillMin.x + padding.x, pillMin.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(GetSceneModeColor(editorState.Scene.Mode)),
            statusText.c_str()
        );

        ImGui::Dummy(pillSize);
    }

    bool DrawMenuAction(Quiet::Editor::EditorActions& actions,
                        Quiet::Editor::EditorActionID actionID)
    {
        const char* shortcut = actions.GetShortcut(actionID);
        if (shortcut[0] == '\0') shortcut = nullptr;

        const bool enabled = actions.CanExecute(actionID);
        const bool pressed = QUI::EditorMenuItem(
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
        DrawTopbarBrand(m_UIScale);
        if (m_Fonts.Bold) ImGui::PopFont();

        ImGui::SameLine();
        DrawTopbarSeparator();

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
        DrawTopbarSeparator();
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

        DrawTopbarStatusPill(editorState, m_UIScale);

        PopToolbarStyle();
        ImGui::End();
    }
}