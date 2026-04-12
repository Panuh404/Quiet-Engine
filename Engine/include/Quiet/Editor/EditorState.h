#pragma once

#include <cstddef>
#include <optional>
#include "Quiet/Core/UUID.h"

namespace Quiet::Editor
{
    enum class GizmoOperation
    {
        Translate,
        Rotate,
        Scale
    };

    enum class GizmoMode
    {
        Local,
        World
    };

    enum class SceneMode
    {
        Edit,
        Simulate
    };

    struct EditorState
    {
        std::optional<Core::UUID> SelectedActorID;

        std::uint32_t ViewportWidth = 0;
        std::uint32_t ViewportHeight = 0;

        bool ViewportFocused = false;
        bool ViewportHovered = false;
        bool ViewportImageHovered = false;

        float ViewportScreenX = 0.0f;
        float ViewportScreenY = 0.0f;

        GizmoOperation CurrentGizmoOperation = GizmoOperation::Translate;
        GizmoMode CurrentGizmoMode = GizmoMode::World;
        SceneMode CurrentSceneMode = SceneMode::Edit;

        bool GizmoSnapEnabled = false;
        float TranslateSnap[3] = { 0.5f, 0.5f, 0.5f };
        float RotateSnap = 15.0f;
        float ScaleSnap = 0.1f;

        bool RequestFocusSelected = false;
        bool RequestNewScene = false;
        bool RequestSaveScene = false;
        bool RequestLoadScene = false;
        bool RequestEnterSimulate = false;
        bool RequestStopSimulate = false;
        bool RequestUndo = false;
        bool RequestRedo = false;

        bool CanUndo = false;
        bool CanRedo = false;
        bool SceneDirty = false;
        std::string SceneDisplayName = "DefaultScene.json";
    };
}
