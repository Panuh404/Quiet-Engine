#include "Quiet/Editor/EditorActions.h"

#include "imgui.h"

#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorController.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Input/Input.h"

namespace Quiet::Editor
{
    EditorActions::EditorActions(Input::Input& input,
                                 Scene::Scene& scene,
                                 EditorCommands& commands,
                                 EditorState& state)
        : m_Input(input)
        , m_Scene(scene)
        , m_Commands(commands)
        , m_State(state)
    {
    }

    const char* EditorActions::GetLabel(EditorActionID actionID) const
    {
        switch (actionID)
        {
            case EditorActionID::NewScene:          return "New Scene";
            case EditorActionID::SaveScene:         return "Save Scene";
            case EditorActionID::LoadScene:         return "Load Scene";
            case EditorActionID::Undo:              return "Undo";
            case EditorActionID::Redo:              return "Redo";
            case EditorActionID::CreateEmptyActor:  return "Create Empty Actor";
            case EditorActionID::DuplicateSelected: return "Duplicate Selected";
            case EditorActionID::DeleteSelected:    return "Delete Selected";
            case EditorActionID::FocusSelected:     return "Focus Selected";

            case EditorActionID::ToggleSimulate:
                return (m_State.CurrentSceneMode == SceneMode::Simulate)
                    ? "Stop Simulate"
                    : "Simulate";

            case EditorActionID::TogglePlay:
                return (m_State.CurrentSceneMode == SceneMode::Play)
                    ? "Stop Play"
                    : "Play";

            default:
                return "Action";
        }
    }

    const char* EditorActions::GetShortcut(EditorActionID actionID) const
    {
        switch (actionID)
        {
            case EditorActionID::NewScene:          return "Ctrl+N";
            case EditorActionID::SaveScene:         return "Ctrl+S";
            case EditorActionID::LoadScene:         return "Ctrl+O";
            case EditorActionID::Undo:              return "Ctrl+Z";
            case EditorActionID::Redo:              return "Ctrl+Y / Ctrl+Shift+Z";
            case EditorActionID::CreateEmptyActor:  return "";
            case EditorActionID::DuplicateSelected: return "Ctrl+D";
            case EditorActionID::DeleteSelected:    return "Delete";
            case EditorActionID::FocusSelected:     return "F";
            case EditorActionID::ToggleSimulate:    return "F6";
            case EditorActionID::TogglePlay:        return "F5";
            default:                                return "";
        }
    }

    bool EditorActions::CanExecute(EditorActionID actionID) const
    {
        const bool isEditMode = (m_State.CurrentSceneMode == SceneMode::Edit);
        const bool isSimulating = (m_State.CurrentSceneMode == SceneMode::Simulate);
        const bool isPlaying = (m_State.CurrentSceneMode == SceneMode::Play);
        const bool hasSelection = m_State.SelectedActorID.has_value();

        switch (actionID)
        {
            case EditorActionID::NewScene:
            case EditorActionID::SaveScene:
            case EditorActionID::LoadScene:
            case EditorActionID::CreateEmptyActor:
                return isEditMode;

            case EditorActionID::Undo:
                return isEditMode && m_State.CanUndo;

            case EditorActionID::Redo:
                return isEditMode && m_State.CanRedo;

            case EditorActionID::DuplicateSelected:
            case EditorActionID::DeleteSelected:
            case EditorActionID::FocusSelected:
                return isEditMode && hasSelection;

            case EditorActionID::ToggleSimulate:
                return isEditMode || isSimulating;

            case EditorActionID::TogglePlay:
                return isEditMode || isPlaying;

            default:
                return false;
        }
    }

    void EditorActions::Execute(EditorActionID actionID)
    {
        if (!CanExecute(actionID)) return;

        switch (actionID)
        {
            case EditorActionID::NewScene:
                m_State.QueueRequest(EditorRequestType::NewScene);
                break;

            case EditorActionID::SaveScene:
                m_State.QueueRequest(EditorRequestType::SaveScene);
                break;

            case EditorActionID::LoadScene:
                m_State.QueueRequest(EditorRequestType::LoadScene);
                break;

            case EditorActionID::Undo:
                m_State.QueueRequest(EditorRequestType::Undo);
                break;

            case EditorActionID::Redo:
                m_State.QueueRequest(EditorRequestType::Redo);
                break;

            case EditorActionID::CreateEmptyActor:
                m_Commands.CreateEmptyActor(m_Scene, "Actor");
                break;

            case EditorActionID::DuplicateSelected:
                m_Commands.DuplicateSelectedActor(m_Scene);
                break;

            case EditorActionID::DeleteSelected:
                m_Commands.DeleteSelectedActor(m_Scene);
                break;

            case EditorActionID::FocusSelected:
                m_State.QueueRequest(EditorRequestType::FocusSelected);
                break;

            case EditorActionID::ToggleSimulate:
                if (m_State.CurrentSceneMode == SceneMode::Edit)
                {
                    m_State.QueueRequest(EditorRequestType::EnterSimulate);
                }
                else if (m_State.CurrentSceneMode == SceneMode::Simulate)
                {
                    m_State.QueueRequest(EditorRequestType::StopSimulate);
                }
                break;

            case EditorActionID::TogglePlay:
                if (m_State.CurrentSceneMode == SceneMode::Edit)
                {
                    m_State.QueueRequest(EditorRequestType::EnterPlay);
                }
                else if (m_State.CurrentSceneMode == SceneMode::Play)
                {
                    m_State.QueueRequest(EditorRequestType::StopPlay);
                }
                break;
        }
    }

    void EditorActions::HandleShortcuts(bool mouseCaptureEnabled)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) return;

        using Input::KeyCode;

        const bool ctrlDown =
            m_Input.IsKeyDown(KeyCode::LeftControl) ||
            m_Input.IsKeyDown(KeyCode::RightControl);

        const bool shiftDown =
            m_Input.IsKeyDown(KeyCode::LeftShift) ||
            m_Input.IsKeyDown(KeyCode::RightShift);

        if (m_Input.IsKeyPressed(KeyCode::F5))
        {
            Execute(EditorActionID::TogglePlay);
            return;
        }
        if (m_Input.IsKeyPressed(KeyCode::F6))
        {
            Execute(EditorActionID::ToggleSimulate);
            return;
        }

        if (mouseCaptureEnabled) return;

        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::N))
        {
            Execute(EditorActionID::NewScene);
            return;
        }
        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::S))
        {
            Execute(EditorActionID::SaveScene);
            return;
        }
        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::O))
        {
            Execute(EditorActionID::LoadScene);
            return;
        }

        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::Z))
        {
            if (shiftDown)
            {
                Execute(EditorActionID::Redo);
            }
            else
            {
                Execute(EditorActionID::Undo);
            }
            return;
        }

        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::Y))
        {
            Execute(EditorActionID::Redo);
            return;
        }

        if (ctrlDown && m_Input.IsKeyPressed(KeyCode::D))
        {
            Execute(EditorActionID::DuplicateSelected);
            return;
        }
        if (m_Input.IsKeyPressed(KeyCode::Delete))
        {
            Execute(EditorActionID::DeleteSelected);
            return;
        }
        if (m_Input.IsKeyPressed(KeyCode::F))
        {
            Execute(EditorActionID::FocusSelected);
            return;
        }
    }
}