#pragma once

#include <cstdint>

namespace Quiet::Input
{
    /**
     * @brief Identifies buttons in GLFW's standard gamepad mapping.
     * @ingroup quiet_input
     *
     * Face buttons use physical positions rather than vendor labels.
     */
    enum class GamepadCode : std::int32_t
    {
        South = 0,
        East = 1,
        West = 2,
        North = 3,
        LeftBumper = 4,
        RightBumper = 5,
        Back = 6,
        Start = 7,
        Guide = 8,
        LeftThumb = 9,
        RightThumb = 10,
        DpadUp = 11,
        DpadRight = 12,
        DpadDown = 13,
        DpadLeft = 14
    };
}