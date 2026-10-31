#include "Quiet/Input/Input.h"

#include "GLFW/glfw3.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Platform/Window.h"


namespace
{
    constexpr int ToNative(Quiet::Input::KeyCode key)       { return static_cast<int>(key); }
    constexpr int ToNative(Quiet::Input::MouseCode button)  { return static_cast<int>(button); }
}

namespace Quiet::Input
{
    Input::Input(const Quiet::Platform::Window& window)
        : m_Window(&window) {}

    void Input::Update()
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());

        m_ConsumedKeys.fill(false);
        m_ConsumedGamepadButtons.fill(false);
        m_ConsumedMouseButtons.fill(false);
        m_PreviousKeys = m_CurrentKeys;
        m_PreviousMouseButtons = m_CurrentMouseButtons;

        for (std::size_t i = 0; i < KeyCount; ++i)
        {
            m_CurrentKeys[i] = (glfwGetKey(handle, static_cast<int>(i)) == GLFW_PRESS);
        }

        for (std::size_t i = 0; i < MouseButtonCount; ++i)
        {
            m_CurrentMouseButtons[i] = (glfwGetMouseButton(handle, static_cast<int>(i)) == GLFW_PRESS);
        }

        static_assert(GamepadButtonCount == GLFW_GAMEPAD_BUTTON_LAST + 1);

        const int previousJoystick = m_GamepadJoystick;
        GLFWgamepadstate gamepadState{};

        const bool existingSample = m_GamepadJoystick >= 0 && glfwGetGamepadState(m_GamepadJoystick, &gamepadState) == GLFW_TRUE;
        if (!existingSample)
        {
            m_GamepadJoystick = -1;
            for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
            {
                if (glfwGetGamepadState(joystick, &gamepadState) == GLFW_TRUE)
                {
                    m_GamepadJoystick = joystick;
                    break;
                }
            }
        }

        m_PreviousGamepadButtons = m_CurrentGamepadButtons;
        m_CurrentGamepadButtons.fill(false);

        if (m_GamepadJoystick >= 0)
        {
            for (std::size_t index = 0; index < GamepadButtonCount; ++index)
            {
                m_CurrentGamepadButtons[index] = gamepadState.buttons[index] == GLFW_PRESS;
            }
        }

        if (m_GamepadJoystick != previousJoystick)
        {
            ++m_GamepadGeneration;
            m_PreviousGamepadButtons = m_CurrentGamepadButtons;
        }

        glfwGetCursorPos(handle, &m_MouseX, &m_MouseY);

        if (m_FirstMouseSample)
        {
            m_LastMouseX = m_MouseX;
            m_LastMouseY = m_MouseY;
            m_FirstMouseSample = false;
        }

        m_MouseDeltaX = m_MouseX - m_LastMouseX;
        m_MouseDeltaY = m_MouseY - m_LastMouseY;

        m_LastMouseX = m_MouseX;
        m_LastMouseY = m_MouseY;
    }


    bool Input::IsKeyDown(KeyCode key) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(key));
        return !m_ConsumedKeys[index] && m_CurrentKeys[index];
    }

    bool Input::IsKeyPressed(KeyCode key) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(key));
        return !m_ConsumedKeys[index] && m_CurrentKeys[index] && !m_PreviousKeys[index];
    }

    void Input::ConsumeKey(KeyCode key) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(key));
        m_ConsumedKeys[index] = true;
    }

    bool Input::IsGamepadButtonDown(GamepadCode button) const
    {
        const auto index = static_cast<std::size_t>(button);
        return !m_ConsumedGamepadButtons[index] &&
               m_CurrentGamepadButtons[index];
    }

    bool Input::IsGamepadButtonPressed(GamepadCode button) const
    {
        const auto index = static_cast<std::size_t>(button);
        return !m_ConsumedGamepadButtons[index] &&
               m_CurrentGamepadButtons[index] &&
               !m_PreviousGamepadButtons[index];
    }

    bool Input::IsGamepadButtonReleased(GamepadCode button) const
    {
        const auto index = static_cast<std::size_t>(button);
        return !m_ConsumedGamepadButtons[index] &&
               !m_CurrentGamepadButtons[index] &&
               m_PreviousGamepadButtons[index];
    }

    void Input::ConsumeGamepadButton(GamepadCode button) noexcept
    {
        const auto index = static_cast<std::size_t>(button);
        m_ConsumedGamepadButtons[index] = true;
    }

    bool Input::IsMouseButtonDown(MouseCode button) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(button));
        return !m_ConsumedMouseButtons[index] && m_CurrentMouseButtons[index];
    }

    bool Input::IsMouseButtonPressed(MouseCode button) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(button));
        return !m_ConsumedMouseButtons[index] && m_CurrentMouseButtons[index] && !m_PreviousMouseButtons[index];
    }

    void Input::ConsumeMouseButton(MouseCode button) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(button));
        m_ConsumedMouseButtons[index] = true;
    }

    std::pair<double, double> Input::GetMousePosition() const
    {
        return { m_MouseX, m_MouseY };
    }

    std::pair<double, double> Input::GetMouseDelta() const
    {
        return { m_MouseDeltaX, m_MouseDeltaY };
    }

    void Input::SetMousePosition(double x, double y)
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
        glfwSetCursorPos(handle, x, y);

        m_MouseX = x;
        m_MouseY = y;

        m_LastMouseX = x;
        m_LastMouseY = y;

        m_MouseDeltaX = 0.0;
        m_MouseDeltaY = 0.0;

        m_FirstMouseSample = false;
    }

    void Input::SetCursorDisabled(bool disabled)
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
        glfwSetInputMode(
            handle,
            GLFW_CURSOR,
            disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );
        QUIET_CORE_INFO("Cursor mode: {}.", disabled ? "disabled" : "normal");
    }

    void Input::EnableRawMouseMotionIfSupported(bool enabled)
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
        if (glfwRawMouseMotionSupported())
        {
            glfwSetInputMode(
                handle,
                GLFW_RAW_MOUSE_MOTION,
                enabled ? GLFW_TRUE : GLFW_FALSE
            );
            QUIET_CORE_INFO("Raw mouse motion: {}.", enabled ? "enabled" : "disabled");
        }
        else
        {
            QUIET_CORE_WARN("Raw mouse motion is not supported on this platform.");
        }
    }

    void Input::ResetMouseDelta()
    {
        m_MouseDeltaX = 0.0;
        m_MouseDeltaY = 0.0;
        m_FirstMouseSample = true;
    }
}
