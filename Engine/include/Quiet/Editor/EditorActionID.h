#pragma once

namespace Quiet::Editor
{
    enum class EditorActionID
    {
        NewScene,
        SaveScene,
        LoadScene,
        Undo,
        Redo,
        CreateEmptyActor,
        DuplicateSelected,
        DeleteSelected,
        FocusSelected,
        ToggleSimulate,
        TogglePlay,
    };
}