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
        [[nodiscard]] bool ShouldClose() const;

        [[nodiscard]] bool IsVulkanSupported() const;
        [[nodiscard]] std::vector<const char*> GetRequiredVulkanInstanceExtensions() const;

        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> GetFramebufferSize() const;

        [[nodiscard]] void* GetNativeHandle() const;
        [[nodiscard]] const WindowDesc& GetDesc() const { return m_Desc; }

    private:
        WindowDesc m_Desc;
        GLFWwindow* m_Handle = nullptr;
    };
}