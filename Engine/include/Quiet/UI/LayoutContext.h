#pragma once

namespace Quiet::Assets
{
    struct FontAtlasData;
}

namespace Quiet::UI
{
    /**
     * @brief Supplies borrowed CPU resources during widget measurement.
     * @ingroup quiet_renderer
     *
     * Resources must remain valid and unchanged throughout Measure().
     * Widgets must not retain these pointers.
     */
    struct LayoutContext
    {
        const Assets::FontAtlasData* DefaultFontAtlas = nullptr;
    };
}