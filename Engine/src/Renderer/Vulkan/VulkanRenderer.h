#pragma once

#include <memory>
#include <optional>
#include <span>

#include <vulkan/vulkan.h>
#include "imgui.h"

#include "Quiet/Assets/FontAtlasData.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Platform/Window.h"
#include "Quiet/UI/PaintCommands.h"


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
    class Texture2D;
    class TextRenderSystem;
    class TextPipeline;
    class PanelPipeline;
    class ImagePipeline;
    class ImageResources;
    class TextureLibrary;
    class CameraResources;
    class StaticMeshPipeline;
    class FrameSync;
    class StaticMeshRenderSystem;
    class StaticMeshMaterialResources;
    class EditorGridPipeline;
    class EditorGridRenderSystem;
    class SelectionMaskPipeline;
    class SelectionMaskRenderSystem;
    class SelectionOutlinePipeline;
    class SelectionOutlineRenderSystem;

    /**
     * @brief Owns Vulkan rendering infrastructure and coordinates frame rendering.
     * @ingroup quiet_renderer
     *
     * Borrows the Window and receives Scene and camera inputs from Application.
     * Owns mesh and texture libraries, frame resources, viewport targets, and UI.
     */
    class VulkanRenderer
    {
    public:
        VulkanRenderer(const Core::AppSpec& spec, Platform::Window& window);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        /**
         * @brief Acquires an image, records and submits rendering, and presents it.
         * @param scene Scene supplied by Application.
         * @param cameraData Camera values uploaded for the acquired image.
         * @param imguiLayer UI layer whose frame has been built by the caller.
         * @param drawEditorGrid Requests Editor grid rendering.
         * @param selectedActorID Optional Actor identity for selection feedback.
         * @param paintCommands Ordered UI operations borrowed during this call.
         * @note Command storage is not retained after this call returns.
         * @return True on the normal successful path; false after resize-driven
         *         or out-of-date/suboptimal presentation recreation.
         * @note True does not establish GPU completion.
         * @note False can occur after graphics submission and does not undo updates.
         * @note Exceptions propagate; retry synchronization is not restored.
         */
        bool DrawFrame(
            Scene::Scene& scene,
            const CameraUniformData& cameraData,
            UI::ImGuiLayer& imguiLayer,
            bool drawEditorGrid,
            std::optional<Core::UUID> selectedActorID,
            std::span<const Quiet::UI::PaintCommand> paintCommands = {});

        /**
         * @brief Requests device idle when the logical device exists.
         * @note The Vulkan result is not checked or returned.
         */
        void WaitIdle() const;

        /**
         * @brief Rebuilds swapchain-dependent rendering resources.
         * @note Waits for nonzero framebuffer dimensions and checks device idle.
         * @note Invalidates references to replaced UI and graphics command objects.
         * @note Clears pending viewport sizing; exceptions provide no rollback.
         */
        void RecreateSwapchain();

        /**
         * @brief Advances or applies the pending Scene viewport resize.
         * @note Different nonzero dimensions require three eligible application calls.
         * @note The device-idle result is not checked.
         */
        void ApplyPendingViewportResize();

        [[nodiscard]] MeshLibrary& GetMeshLibrary() { return *m_MeshLibrary; }
        [[nodiscard]] const MeshLibrary& GetMeshLibrary() const { return *m_MeshLibrary; }

        [[nodiscard]] TextureLibrary& GetTextureLibrary() { return *m_TextureLibrary; }
        [[nodiscard]] const TextureLibrary& GetTextureLibrary() const { return *m_TextureLibrary; }

        /**
         * @brief Borrows the CPU atlas used by default runtime text rendering.
         * @note The renderer owns the atlas; do not retain beyond its lifetime.
         */
        [[nodiscard]] const Assets::FontAtlasData& GetDefaultFontAtlas() const noexcept { return m_DefaultFontAtlas; }

        [[nodiscard]] UI::ImGuiLayer& GetImGuiLayer() { return *m_ImGuiLayer; }
        [[nodiscard]] const UI::ImGuiLayer& GetImGuiLayer() const { return *m_ImGuiLayer; }

        /**
         * @brief Obtains the UI texture identifier for a ready Scene target.
         * @return Empty when the layer or target is unavailable, invalid, or unready.
         * @note Do not retain the identifier across target or UI recreation.
         */
        [[nodiscard]] ImTextureID GetSceneViewportTextureID();

        /**
         * @brief Reports the renderer's Scene viewport readiness flag.
         * @note Set during Scene command recording, before GPU execution completes.
         * @note Not a submission fence or GPU completion query.
         */
        [[nodiscard]] bool IsSceneViewportReady() const { return m_SceneViewportReady; }

        [[nodiscard]] const PhysicalDevice& GetPhysicalDevice() const { return *m_PhysicalDevice; }
        [[nodiscard]] const LogicalDevice& GetLogicalDevice() const { return *m_LogicalDevice; }
        [[nodiscard]] const GraphicsCommands& GetGraphicsCommands() const { return *m_GraphicsCommands; }

        [[nodiscard]] StaticMeshMaterialResources& GetStaticMeshMaterialResources() const { return *m_StaticMeshMaterialResources; }

        [[nodiscard]] VkExtent2D GetSwapchainExtent() const;
        [[nodiscard]] VkExtent2D GetSceneViewportExtent() const;

        /**
         * @brief Stores a deferred Scene viewport size request.
         * @note Changed dimensions reset the application-call stability counter.
         * @note Zero dimensions remain pending without resizing.
         */
        void RequestViewportResize(std::uint32_t width, std::uint32_t height);

    private:
        void CreateRenderResources(Platform::Window& window, const Core::AppSpec& spec);
        void DestroyRenderResources();

        void LoadEditorTextures();

        void EnsureSceneViewportSize();
        void RecordFrameCommandBuffer(
            std::uint32_t imageIndex,
            Scene::Scene& scene,
            UI::ImGuiLayer& imguiLayer,
            bool drawEditorGrid,
            const std::optional<Core::UUID>& selectedActorID,
            std::span<const Quiet::UI::PaintCommand> paintCommands);
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
        std::unique_ptr<TextureLibrary> m_TextureLibrary;
        Assets::FontAtlasData m_DefaultFontAtlas;
        std::unique_ptr<Texture2D> m_DefaultFontTexture;
        std::unique_ptr<TextRenderSystem> m_TextRenderSystem;
        std::unique_ptr<TextPipeline> m_TextPipeline;
        std::unique_ptr<PanelPipeline> m_PanelPipeline;
        std::unique_ptr<ImagePipeline> m_ImagePipeline;
        std::unique_ptr<ImageResources> m_ImageResources;
        std::unique_ptr<CameraResources> m_CameraResources;
        std::unique_ptr<StaticMeshPipeline> m_StaticMeshPipeline;
        std::unique_ptr<FrameSync> m_FrameSync;
        std::unique_ptr<StaticMeshRenderSystem> m_StaticMeshRenderSystem;
        std::unique_ptr<StaticMeshMaterialResources> m_StaticMeshMaterialResources;
        std::unique_ptr<EditorGridPipeline> m_EditorGridPipeline;
        std::unique_ptr<EditorGridRenderSystem> m_EditorGridRenderSystem;
        std::unique_ptr<SelectionMaskPipeline> m_SelectionMaskPipeline;
        std::unique_ptr<SelectionMaskRenderSystem> m_SelectionMaskRenderSystem;
        std::unique_ptr<SelectionOutlinePipeline> m_SelectionOutlinePipeline;
        std::unique_ptr<SelectionOutlineRenderSystem> m_SelectionOutlineRenderSystem;

        std::uint32_t m_PendingViewportWidth = 0;
        std::uint32_t m_PendingViewportHeight = 0;
        std::uint32_t m_ViewportResizeStableFrames = 0;
        bool m_ViewportResizePending = false;
        bool m_SceneViewportReady = false;
        bool m_EditorMode = false;
    };
}