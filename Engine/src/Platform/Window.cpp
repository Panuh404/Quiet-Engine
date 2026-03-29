#include "Quiet/Platform/Window.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace
{
    void FramebufferSizeCallback(GLFWwindow* window, int, int)
    {
        auto* quietWindow = static_cast<Quiet::Platform::Window*>(glfwGetWindowUserPointer(window));
        if (quietWindow)
        {
            quietWindow->MarkFramebufferResized();
        }
    }
}


namespace Quiet::Platform
{
    Window::Window(const WindowDesc &desc) : m_Desc(desc)
    {
        if (glfwInit() != GLFW_TRUE)
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, m_Desc.Resizable ? GLFW_TRUE : GLFW_FALSE);

        m_Handle = glfwCreateWindow(
            static_cast<int>(m_Desc.Width),
            static_cast<int>(m_Desc.Height),
            m_Desc.Title.c_str(),
            nullptr,
            nullptr);

        if (!m_Handle)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(m_Handle, this);
        glfwSetFramebufferSizeCallback(m_Handle, FramebufferSizeCallback);
    }

    Window::~Window()
    {
        if (m_Handle)
        {
            glfwDestroyWindow(m_Handle);
            m_Handle = nullptr;
        }
        glfwTerminate();
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

    void Window::WaitEvents()
    {
        glfwWaitEvents();
    }

    std::vector<const char*> Window::GetRequiredVulkanInstanceExtensions() const
    {
        std::uint32_t count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);

        if (!extensions)
        {
            return {};
        }
        return { extensions, extensions + count };
    }

    std::pair<std::uint32_t, std::uint32_t> Window::GetFramebufferSize() const
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_Handle, &width, &height);

        return {
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)
        };
    }

    bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Handle) == GLFW_TRUE; }
    bool Window::IsVulkanSupported() const { return glfwVulkanSupported() == GLFW_TRUE; }
    void* Window::GetNativeHandle() const  { return m_Handle; }
}
