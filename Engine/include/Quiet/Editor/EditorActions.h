#pragma once

#include "EditorActionID.h"

namespace Quiet::Editor
{
    class EditorCommands;
    class EditorController;
    struct EditorState;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Editor
{
    class EditorActions
    {
    public:
        EditorActions(Input::Input& input,
                      Scene::Scene& scene,
                      EditorCommands& commands,
                      EditorState& state);

        [[nodiscard]] const char* GetLabel(EditorActionID actionID) const;
        [[nodiscard]] const char* GetShortcut(EditorActionID actionID) const;
        [[nodiscard]] bool CanExecute(EditorActionID actionID) const;

        void Execute(EditorActionID actionID);
        void HandleShortcuts(bool mouseCaptureEnabled);

    private:
        Input::Input& m_Input;
        Scene::Scene& m_Scene;
        EditorCommands& m_Commands;
        EditorState& m_State;
    };
}