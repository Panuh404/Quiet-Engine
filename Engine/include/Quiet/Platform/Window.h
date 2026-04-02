#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

struct GLFWwindow;

namespace Quiet::Platform
{
    struct WindowDesc
    {
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
        std::string Title = "Quiet";
        bool Resizable = true;
    };

    class Window
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        void PollEvents();
        void WaitEvents();

        [[nodiscard]] bool ShouldClose() const;

        [[nodiscard]] bool IsVulkanSupported() const;
        [[nodiscard]] std::vector<const char*> GetRequiredVulkanInstanceExtensions() const;

        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> GetFramebufferSize() const;
        [[nodiscard]] bool WasFramebufferResized() const { return m_FramebufferResized; }

        void ResetFramebufferResizedFlag() { m_FramebufferResized = false; }
        void MarkFramebufferResized() { m_FramebufferResized = true; }
        friend void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

        [[nodiscard]] void* GetNativeHandle() const;
        [[nodiscard]] const WindowDesc& GetDesc() const { return m_Desc; }

        bool IsKeyDown(int key) const;
        std::pair<double, double> GetCursorPosition() const;
        void SetCursorDisabled(bool disabled);
        void EnableRawMouseMotionIfSupported(bool enabled);

    private:
        WindowDesc m_Desc;
        GLFWwindow* m_Handle = nullptr;
        bool m_FramebufferResized = false;
    };
}