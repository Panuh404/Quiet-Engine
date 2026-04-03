#include "Input.h"

#include "GLFW/glfw3.h"
#include "Quiet/Platform/Window.h"

namespace Quiet::Input
{
    Input::Input(const Quiet::Platform::Window& window)
        : m_Window(&window) {}

    void Input::Update()
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
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
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
        return glfwGetKey(handle, static_cast<int>(key)) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonDown(MouseCode button) const
    {
        auto* handle = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
        return glfwGetMouseButton(handle, static_cast<int>(button)) == GLFW_PRESS;
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
        }
    }

    void Input::ResetMouseDelta()
    {
        m_MouseDeltaX = 0.0;
        m_MouseDeltaY = 0.0;
    }
}
