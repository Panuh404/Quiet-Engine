#pragma once

#include <cstddef>
#include <optional>

namespace Quiet::Core
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

    struct EditorState
    {
        std::optional<std::size_t> SelectedActorIndex;

        std::uint32_t ViewportWidth = 0;
        std::uint32_t ViewportHeight = 0;

        bool ViewportFocused = false;
        bool ViewportHovered = false;
        bool ViewportImageHovered = false;

        float ViewportScreenX = 0.0f;
        float ViewportScreenY = 0.0f;

        GizmoOperation CurrentGizmoOperation = GizmoOperation::Translate;
        GizmoMode CurrentGizmoMode = GizmoMode::World;

        bool GizmoSnapEnabled = false;
        float TranslateSnap[3] = { 0.5f, 0.5f, 0.5f };
        float RotateSnap = 15.0f;
        float ScaleSnap = 0.1f;

        bool RequestFocusSelected = false;
    };
}
