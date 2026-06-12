#pragma once

#include <cstdint>
#include <functional>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "glm/vec3.hpp"

#include "ImGuiFonts.h"

struct GLFWwindow;
struct ImDrawData;

namespace Quiet::Editor
{
    class EditorActions;
    class EditorCommands;
    class EditorController;
    struct EditorState;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Project
{
    struct ProjectContext;
    struct ProjectRegistry;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class MeshLibrary;
    class TextureLibrary;
}

namespace Quiet::Renderer::UI
{
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
        void BuildEditorUI(const ImGuiStats &stats,
                           Input::Input &input,
                           Scene::Scene &scene,
                           Editor::EditorController &editorController,
                           Editor::EditorState &editorState,
                           Project::ProjectContext &projectContext,
                           const Project::ProjectRegistry &projectRegistry,
                           const Vulkan::MeshLibrary &meshLibrary,
                           const Vulkan::TextureLibrary &textureLibrary,
                           ImTextureID sceneViewportTexture,
                           const std::function<void()> &viewportOverlayCallback);
        void BuildRuntimeUI(std::uint32_t& outViewportWidth,
                            std::uint32_t& outViewportHeight,
                            ImTextureID sceneViewportTexture);
        void EndFrame();
        void Render(VkCommandBuffer commandBuffer) const;

        [[nodiscard]] bool WantsMouseCapture() const;
        [[nodiscard]] bool WantsKeyboardCapture() const;

        [[nodiscard]] float GetUIScale() const { return m_UIScale; }
        [[nodiscard]] float Scale(float value) const { return value * m_UIScale; }
        [[nodiscard]] ImVec2 Scale(const ImVec2& value) const
        {
            return {value.x * m_UIScale, value.y * m_UIScale};
        }

        [[nodiscard]] ImTextureID GetOrCreateSceneViewportTextureID(
            VkSampler sampler,
            VkImageView imageView,
            VkImageLayout imageLayout
        );
        void ClearSceneViewportTexture();

    private:
        [[nodiscard]] float GetToolbarHeight() const;

        // Dockspace
        void BuildDockspace();
        void ApplyDefaultDockLayout(ImGuiID dockspaceID, const ImVec2& dockspaceSize);
        [[nodiscard]] bool ShouldApplyDefaultDockLayout() const;

        // ImGui Chrome
        void BuildMainMenuBar(Editor::EditorActions& editorActions,
                              Editor::EditorState& editorState);
        void BuildTopbar(Editor::EditorActions& editorActions,
                        Editor::EditorState& editorState,
                        const Vulkan::TextureLibrary& textureLibrary);

        // Scene Panels
        void BuildInspector(Scene::Scene& scene,
                            Editor::EditorCommands& editorCommands,
                            Editor::EditorState& editorState,
                            const Project::ProjectContext& projectContext,
                            const Vulkan::MeshLibrary& meshLibrary,
                            const Vulkan::TextureLibrary& textureLibrary);
        void BuildSceneSettings(Scene::Scene& scene,
                                Editor::EditorCommands& editorCommands,
                                Editor::EditorState& editorState);
        void BuildHierarchy(Scene::Scene& scene,
                            Editor::EditorActions& editorActions,
                            Editor::EditorCommands& editorCommands,
                            Editor::EditorState& editorState,
                            const Project::ProjectContext& projectContext,
                            const Vulkan::MeshLibrary& meshLibrary);
        void BuildAssetsPanel(Project::ProjectContext& projectContext,
                              Editor::EditorState& editorState,
                              Scene::Scene& scene,
                              const Vulkan::MeshLibrary& meshLibrary,
                              const Vulkan::TextureLibrary& textureLibrary);

        // Viewport Panels
        void BuildViewportToolbar(Editor::EditorState& editorState,
                          const ImVec2& imageMin);

        void BuildViewport(Editor::EditorState& editorState,
                           ImTextureID sceneViewportTexture,
                           const std::function<void()>& viewportOverlayCallback);

        void BuildProjectManagerPopup(Editor::EditorState& editorState,
                                      const Project::ProjectContext& projectContext,
                                      const Project::ProjectRegistry& projectRegistry);

        // Diagnostics
        void BuildDebugWindow(const ImGuiStats& stats);
        void BuildConsoleWindow();

        void CreateDescriptorPool();
        void DestroyDescriptorPool();

    private:
        EditorFonts m_Fonts{};
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkFormat m_ImGuiColorAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkImageView m_SceneViewportImageView = VK_NULL_HANDLE;
        VkSampler m_SceneViewportSampler = VK_NULL_HANDLE;
        VkDescriptorSet m_SceneViewportDescriptorSet = VK_NULL_HANDLE;
        bool m_DefaultLayoutApplied = false;
        bool m_RequestResetLayout = false;
        float m_UIScale = 1.0f;
    };
}