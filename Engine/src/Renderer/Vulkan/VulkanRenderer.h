#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "imgui.h"

#include "Quiet/Platform/Window.h"

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Scene
{
    class Scene;
    class FreeCamera;
}

namespace Quiet::Core
{
    struct AppSpec;
}

namespace Quiet::Editor
{
    struct EditorState;
}

namespace Quiet::Renderer::UI
{
    class ImGuiLayer;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
    class VulkanInstance;
    class VulkanSurface;
    class PhysicalDevice;
    class LogicalDevice;
    class Swapchain;
    class DepthResources;
    class SceneViewportTarget;
    class GraphicsCommands;
    class MeshLibrary;
    class CameraResources;
    class StaticMeshPipeline;
    class FrameSync;
    class StaticMeshRenderSystem;

    class VulkanRenderer
    {
    public:
        VulkanRenderer(const Core::AppSpec& spec, Platform::Window& window);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        bool DrawFrame(Scene::Scene& scene,
                       const CameraUniformData& cameraData,
                       UI::ImGuiLayer& imguiLayer);

        void RecreateSwapchain();
        void ApplyPendingViewportResize();

        [[nodiscard]] MeshLibrary& GetMeshLibrary() { return *m_MeshLibrary; }
        [[nodiscard]] const MeshLibrary& GetMeshLibrary() const { return *m_MeshLibrary; }

        [[nodiscard]] UI::ImGuiLayer& GetImGuiLayer() { return *m_ImGuiLayer; }
        [[nodiscard]] const UI::ImGuiLayer& GetImGuiLayer() const { return *m_ImGuiLayer; }

        [[nodiscard]] ImTextureID GetSceneViewportTextureID() const;
        [[nodiscard]] bool IsSceneViewportReady() const { return m_SceneViewportReady; }

        [[nodiscard]] const PhysicalDevice& GetPhysicalDevice() const { return *m_PhysicalDevice; }
        [[nodiscard]] const LogicalDevice& GetLogicalDevice() const { return *m_LogicalDevice; }
        [[nodiscard]] const GraphicsCommands& GetGraphicsCommands() const { return *m_GraphicsCommands; }

        [[nodiscard]] VkExtent2D GetSwapchainExtent() const;
        [[nodiscard]] VkExtent2D GetSceneViewportExtent() const;

        void RequestViewportResize(std::uint32_t width, std::uint32_t height);

    private:
        void CreateRenderResources(Platform::Window& window, const Core::AppSpec& spec);
        void DestroyRenderResources();

        void EnsureSceneViewportSize();
        void RecordFrameCommandBuffer(std::uint32_t imageIndex,
                                      Scene::Scene& scene,
                                      UI::ImGuiLayer& imguiLayer);

    private:
        Platform::Window* m_Window = nullptr;

        std::unique_ptr<VulkanInstance> m_VulkanInstance;
        std::unique_ptr<VulkanSurface> m_VulkanSurface;
        std::unique_ptr<PhysicalDevice> m_PhysicalDevice;
        std::unique_ptr<LogicalDevice> m_LogicalDevice;
        std::unique_ptr<Swapchain> m_Swapchain;
        std::unique_ptr<DepthResources> m_DepthResources;
        std::unique_ptr<SceneViewportTarget> m_SceneViewportTarget;
        std::unique_ptr<GraphicsCommands> m_GraphicsCommands;
        std::unique_ptr<UI::ImGuiLayer> m_ImGuiLayer;
        std::unique_ptr<MeshLibrary> m_MeshLibrary;
        std::unique_ptr<CameraResources> m_CameraResources;
        std::unique_ptr<StaticMeshPipeline> m_StaticMeshPipeline;
        std::unique_ptr<FrameSync> m_FrameSync;
        std::unique_ptr<StaticMeshRenderSystem> m_StaticMeshRenderSystem;

        std::uint32_t m_PendingViewportWidth = 0;
        std::uint32_t m_PendingViewportHeight = 0;
        std::uint32_t m_ViewportResizeStableFrames = 0;
        bool m_ViewportResizePending = false;
        bool m_SceneViewportReady = false;
    };
}