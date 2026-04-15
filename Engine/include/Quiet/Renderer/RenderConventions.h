#pragma once

namespace Quiet::Renderer
{
    // Used by the Vulkan render path when rendering the scene to the viewport target.
    inline constexpr bool UseRenderProjectionYFlip = true;

    // Used by editor screen-space tools.
    inline constexpr bool UseEditorScreenProjectionYFlip = false;
}