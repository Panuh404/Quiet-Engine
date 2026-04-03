#pragma once

#include <utility>

#include "KeyCodes.h"
#include "MouseCodes.h"

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Input
{
    class Input
    {
    public:
        explicit Input(const Quiet::Platform::Window& window);

        void Update();

        bool IsKeyDown(KeyCode key) const;
        bool IsMouseButtonDown(MouseCode button) const;

        std::pair<double, double> GetMousePosition() const;
        std::pair<double, double> GetMouseDelta() const;

        void SetCursorDisabled(bool disabled);
        void EnableRawMouseMotionIfSupported(bool enabled);
        void ResetMouseDelta();

    private:
        const Quiet::Platform::Window* m_Window = nullptr;
        bool m_FirstMouseSample = true;

        double m_MouseX = 0.0;
        double m_MouseY = 0.0;
        double m_LastMouseX = 0.0;
        double m_LastMouseY = 0.0;
        double m_MouseDeltaX = 0.0;
        double m_MouseDeltaY = 0.0;
    };
}