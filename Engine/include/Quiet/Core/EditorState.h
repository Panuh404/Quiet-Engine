#pragma once
#include <optional>

namespace Quiet::Core
{
    struct EditorState
    {
        std::optional<std::size_t> SelectedActorIndex;
    };
}
