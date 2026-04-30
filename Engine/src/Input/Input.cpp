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
        return m_CurrentKeys[index];
    }

    bool Input::IsKeyPressed(KeyCode key) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(key));
        return m_CurrentKeys[index] && !m_PreviousKeys[index];
    }

    bool Input::IsMouseButtonDown(MouseCode button) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(button));
        return m_CurrentMouseButtons[index];
    }

    bool Input::IsMouseButtonPressed(MouseCode button) const
    {
        const std::size_t index = static_cast<std::size_t>(ToNative(button));
        return m_CurrentMouseButtons[index] && !m_PreviousMouseButtons[index];
    }

    std::pair<double, double> Input::GetMousePosition() const
    {
        return { m_MouseX, m_MouseY };
    }

    std::pair<double, double> Input::GetMouseDelta() const
    {
        return { m_MouseDeltaX, m_MouseDeltaY };
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
