#pragma once

#include <cstdint>
#include <functional>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "glm/vec3.hpp"

struct GLFWwindow;
struct ImDrawData;

namespace Quiet::Core
{
    struct EditorState;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    struct ImGuiStats
    {
        float FrameTimeMs = 0.0f;
        float FPS = 0.0f;
        std::uint32_t ActorCount = 0;
        std::uint32_t MeshCount = 0;
        VkExtent2D SwapchainExtent{};
        bool MouseCaptured = false;
        glm::vec3 CameraPosition = glm::vec3(0.0f);
    };

    class ImGuiLayer
    {
    public:
        ImGuiLayer(GLFWwindow* window,
                   VkInstance instance,
                   VkPhysicalDevice physicalDevice,
                   VkDevice device,
                   std::uint32_t queueFamily,
                   VkQueue queue,
                   std::uint32_t minImageCount,
                   std::uint32_t imageCount,
                   VkFormat colorFormat,
                   VkFormat depthFormat);
        ~ImGuiLayer();

        ImGuiLayer(const ImGuiLayer&) = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;

        void BeginFrame();
        void BuildEditorUI(const ImGuiStats& stats,
                           Scene::Scene& scene,
                           Core::EditorState& editorState,
                           ImTextureID sceneViewportTexture,
                           const std::function<void()>& viewportOverlayCallback);
        void EndFrame();
        void Render(VkCommandBuffer commandBuffer) const;

        [[nodiscard]] bool WantsMouseCapture() const;
        [[nodiscard]] bool WantsKeyboardCapture() const;

    private:
        float GetToolbarHeight() const;
        void BuildToolbar(Core::EditorState& editorState,
                          Scene::Scene& scene);

        void CreateDescriptorPool();
        void DestroyDescriptorPool();
        void BuildDockspace();

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    };
}