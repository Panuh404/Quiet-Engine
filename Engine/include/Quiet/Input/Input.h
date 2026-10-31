#pragma once

#include <array>
#include <utility>

#include "KeyCodes.h"
#include "MouseCodes.h"
#include "GamepadCodes.h"

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

        /**
         * @brief Hides a key from subsequent queries until Update().
         * @note Affects IsKeyDown() and IsKeyPressed() without changing
         *       physical current/previous samples.
         * @pre key must identify a supported key.
         */
        void ConsumeKey(KeyCode key) noexcept;

        /**
         * @brief Reports whether a mapped gamepad was sampled this frame.
         */
        [[nodiscard]] bool IsGamepadConnected() const noexcept { return m_GamepadJoystick >= 0; }

        /**
         * @brief Identifies changes to the selected gamepad connection.
         * @note Changes on detected connection, disconnection, or replacement.
         */
        [[nodiscard]] std::uint64_t GetGamepadGeneration() const noexcept { return m_GamepadGeneration; }

        /**
         * @brief Queries the selected mapped gamepad.
         * @note Returns false without a connected mapped gamepad.
         *       Newly selected devices do not generate pressed/released edges
         *       from their initial sample.
         * @pre button must identify a supported gamepad button.
         */
        [[nodiscard]] bool IsGamepadButtonDown(GamepadCode button) const;
        [[nodiscard]] bool IsGamepadButtonPressed(GamepadCode button) const;
        [[nodiscard]] bool IsGamepadButtonReleased(GamepadCode button) const;

        /**
         * @brief Hides a gamepad button from queries until Update().
         * @note Preserves physical samples and transition history.
         * @pre button must identify a supported gamepad button.
         */
        void ConsumeGamepadButton(GamepadCode button) noexcept;

        [[nodiscard]] bool IsMouseButtonDown(MouseCode button) const;
        [[nodiscard]] bool IsMouseButtonPressed(MouseCode button) const;

        /**
         * @brief Hides a mouse button from subsequent queries until Update().
         * @note Preserves physical button history.
         *       Affects IsMouseButtonDown() and IsMouseButtonPressed().
         * @pre button must identify a supported mouse button.
         */
        void ConsumeMouseButton(MouseCode button) noexcept;

        [[nodiscard]] std::pair<double, double> GetMousePosition() const;
        [[nodiscard]] std::pair<double, double> GetMouseDelta() const;

        void SetMousePosition(double x, double y);
        void SetCursorDisabled(bool disabled);
        void EnableRawMouseMotionIfSupported(bool enabled);
        void ResetMouseDelta();

    private:
        static constexpr std::size_t KeyCount = 512;
        static constexpr std::size_t GamepadButtonCount = 15;
        static constexpr std::size_t MouseButtonCount = 8;

    private:
        const Platform::Window* m_Window = nullptr;

        std::array<bool, KeyCount> m_CurrentKeys{};
        std::array<bool, KeyCount> m_PreviousKeys{};
        std::array<bool, KeyCount> m_ConsumedKeys{};

        std::array<bool, MouseButtonCount> m_CurrentMouseButtons{};
        std::array<bool, MouseButtonCount> m_PreviousMouseButtons{};
        std::array<bool, MouseButtonCount> m_ConsumedMouseButtons{};

        int m_GamepadJoystick = -1;
        std::uint64_t m_GamepadGeneration = 0;

        std::array<bool, GamepadButtonCount> m_CurrentGamepadButtons{};
        std::array<bool, GamepadButtonCount> m_PreviousGamepadButtons{};
        std::array<bool, GamepadButtonCount> m_ConsumedGamepadButtons{};

        bool m_FirstMouseSample = true;

        double m_MouseX = 0.0;
        double m_MouseY = 0.0;
        double m_LastMouseX = 0.0;
        double m_LastMouseY = 0.0;
        double m_MouseDeltaX = 0.0;
        double m_MouseDeltaY = 0.0;
    };
}