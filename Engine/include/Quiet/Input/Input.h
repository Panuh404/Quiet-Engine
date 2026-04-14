#pragma once

#include <array>
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
        explicit Input(const Platform::Window& window);

        void Update();

        [[nodiscard]] bool IsKeyDown(KeyCode key) const;
        [[nodiscard]] bool IsKeyPressed(KeyCode key) const;

        [[nodiscard]] bool IsMouseButtonDown(MouseCode button) const;
        [[nodiscard]] bool IsMouseButtonPressed(MouseCode button) const;


        [[nodiscard]] std::pair<double, double> GetMousePosition() const;
        [[nodiscard]] std::pair<double, double> GetMouseDelta() const;

        void SetCursorDisabled(bool disabled);
        void EnableRawMouseMotionIfSupported(bool enabled);
        void ResetMouseDelta();

    private:
        static constexpr std::size_t KeyCount = 512;
        static constexpr std::size_t MouseButtonCount = 8;

    private:
        const Platform::Window* m_Window = nullptr;

        std::array<bool, KeyCount> m_CurrentKeys{};
        std::array<bool, KeyCount> m_PreviousKeys{};

        std::array<bool, MouseButtonCount> m_CurrentMouseButtons{};
        std::array<bool, MouseButtonCount> m_PreviousMouseButtons{};

        bool m_FirstMouseSample = true;

        double m_MouseX = 0.0;
        double m_MouseY = 0.0;
        double m_LastMouseX = 0.0;
        double m_LastMouseY = 0.0;
        double m_MouseDeltaX = 0.0;
        double m_MouseDeltaY = 0.0;
    };
}