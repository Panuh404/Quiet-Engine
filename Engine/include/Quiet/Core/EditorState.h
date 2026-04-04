#pragma once
#include <optional>

namespace Quiet::Core
{
    struct EditorState
    {
        std::optional<std::size_t> SelectedActorIndex;

        std::uint32_t ViewportWidth = 0;
        std::uint32_t ViewportHeight = 0;
        bool ViewportFocused = false;
        bool ViewportHovered = false;
    };
}
