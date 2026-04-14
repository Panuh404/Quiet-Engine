#pragma once

#include <cstdint>
#include <functional>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "glm/vec3.hpp"

#include "Renderer/Vulkan/ImGuiFonts.h"

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

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class MeshLibrary;

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
                           Input::Input& input,
                           Scene::Scene& scene,
                           Editor::EditorController& editorController,
                           Editor::EditorState& editorState,
                           const MeshLibrary& meshLibrary,
                           ImTextureID sceneViewportTexture,
                           const std::function<void()>& viewportOverlayCallback);
        void EndFrame();
        void Render(VkCommandBuffer commandBuffer) const;

        [[nodiscard]] bool WantsMouseCapture() const;
        [[nodiscard]] bool WantsKeyboardCapture() const;

    private:
        [[nodiscard]] float GetToolbarHeight() const;

        void BuildMainMenuBar(Editor::EditorActions& editorActions);
        void BuildDockspace();
        void BuildTopbar(Editor::EditorActions& editorActions,
                         Editor::EditorState& editorState);
        void BuildViewportToolbar(Editor::EditorState& editorState,
                                  const ImVec2& imageMin);
        void BuildConsoleWindow();
        void BuildInspector(Scene::Scene& scene,
                            Editor::EditorCommands& editorCommands,
                            Editor::EditorState& editorState,
                            const MeshLibrary& meshLibrary);
        void BuildHierarchy(Scene::Scene& scene,
                            Editor::EditorActions& editorActions,
                            Editor::EditorState& editorState);

        void CreateDescriptorPool();
        void DestroyDescriptorPool();

    private:
        EditorFonts m_Fonts{};
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    };
}