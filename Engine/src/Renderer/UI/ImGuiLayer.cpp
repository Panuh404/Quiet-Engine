#include "Renderer/UI/ImGuiLayer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <algorithm>

#include "ImGuiToolbarCommon.h"
#include "Audio/AudioSubsystem.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiTexturePreview.h"

#include "Quiet/Core/Log.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorController.h"
#include "Quiet/Project/ProjectContext.h"

namespace Quiet::Renderer::UI
{
    ImGuiLayer::ImGuiLayer(GLFWwindow* window,
                           VkInstance instance,
                           VkPhysicalDevice physicalDevice,
                           VkDevice device,
                           std::uint32_t queueFamily,
                           VkQueue queue,
                           std::uint32_t minImageCount,
                           std::uint32_t imageCount,
                           VkFormat colorFormat,
                           VkFormat depthFormat,
                           const char* iniFilename,
                           bool enableDocking,
                           bool enableViewports)
        : m_Device(device)
    {
        CreateDescriptorPool();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = iniFilename;
        if (enableDocking)
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        if (enableViewports)
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        float contentScaleX = 1.0f;
        float contentScaleY = 1.0f;
        glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);

#if defined(_WIN32)
        m_UIScale = std::clamp(std::max(contentScaleX, contentScaleY), 1.0f, 2.0f);
#else
        // On Linux/Wayland/KDE, desktop scaling is already applied by the compositor.
        // Scaling ImGui again causes oversized/double-scaled UI.
        m_UIScale = 1.0f;
#endif

        QUIET_CORE_INFO(
            "ImGui scale: contentScale={:.2f},{:.2f} uiScale={:.2f}",
            contentScaleX,
            contentScaleY,
            m_UIScale
        );

        constexpr float kBaseUIFontSize = 16.0f;
        constexpr float kBaseBoldFontSize = 16.0f;
        constexpr float kBaseMonoFontSize = 14.0f;

        m_Fonts = SetupEditorFonts(
            io,
            kBaseUIFontSize * m_UIScale,
            kBaseBoldFontSize * m_UIScale,
            kBaseMonoFontSize * m_UIScale
        );

        ApplyTheme(io, m_UIScale);
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_3;
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = queue;
        initInfo.DescriptorPool = m_DescriptorPool;
        initInfo.MinImageCount = minImageCount;
        initInfo.ImageCount = imageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.CheckVkResultFn = nullptr;

        if (colorFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("ImGuiLayer received undefined color attachment format.");

        m_ImGuiColorAttachmentFormat = colorFormat;

        VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &m_ImGuiColorAttachmentFormat;
        pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.PipelineRenderingCreateInfo = pipelineRenderingInfo;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            throw std::runtime_error("Failed to initialize ImGui Vulkan backend.");
        }

        QUIET_CORE_INFO("Initialized Dear ImGui GLFW + Vulkan debug overlay.");
    }

    ImGuiLayer::~ImGuiLayer()
    {
        ClearSceneViewportTexture();
        ClearTexturePreviewCache(true);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        DestroyDescriptorPool();
    }

    void ImGuiLayer::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::BuildEditorUI(const ImGuiStats &stats,
                                   Input::Input &input,
                                   Scene::Scene &scene,
                                   Editor::EditorController &editorController,
                                   Editor::EditorState &editorState,
                                   Project::ProjectContext &projectContext,
                                   const Project::ProjectRegistry &projectRegistry,
                                   const Vulkan::MeshLibrary &meshLibrary,
                                   const Vulkan::TextureLibrary &textureLibrary,
                                   Audio::AudioSubsystem& audioSubsystem,
                                   ImTextureID sceneViewportTexture,
                                   const std::function<void()> &viewportOverlayCallback)
    {
        Editor::EditorCommands commands(editorController);
        Editor::EditorActions actions(input, scene, commands, editorState);
        editorController.SynchronizeScriptSources(scene, projectContext);

        BuildDockspace();
        BuildTopbar(actions, editorState, textureLibrary);
        BuildHierarchy(scene, actions, commands, editorState, projectContext, meshLibrary);
        BuildInspector(scene, commands, editorState, projectContext, meshLibrary, textureLibrary);
        BuildSceneSettings(scene, commands, editorState);

        BuildAssetsPanel(projectContext, editorState, scene,  meshLibrary, textureLibrary, audioSubsystem);
        BuildMainMenuBar(actions, editorState);
        BuildProjectManagerPopup(editorState, projectContext, projectRegistry);

        BuildDebugWindow(stats);
        BuildViewport( editorState, projectContext, meshLibrary, sceneViewportTexture, viewportOverlayCallback);
        BuildConsoleWindow();

        actions.HandleShortcuts(stats.MouseCaptured);
    }


    void ImGuiLayer::EndFrame()
    {
        ImGui::Render();
    }

    void ImGuiLayer::Render(VkCommandBuffer commandBuffer) const
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

    bool ImGuiLayer::WantsMouseCapture() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiLayer::WantsKeyboardCapture() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    ImTextureID ImGuiLayer::GetOrCreateSceneViewportTextureID(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
    {
        if (sampler == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE)
            return ImTextureID{};

        if (m_SceneViewportDescriptorSet != VK_NULL_HANDLE &&
            m_SceneViewportSampler == sampler &&
            m_SceneViewportImageView == imageView)
            return reinterpret_cast<ImTextureID>(m_SceneViewportDescriptorSet);

        ClearSceneViewportTexture();

        m_SceneViewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
            sampler,
            imageView,
            imageLayout
        );

        if (m_SceneViewportDescriptorSet == VK_NULL_HANDLE)
            return ImTextureID{};

        m_SceneViewportSampler = sampler;
        m_SceneViewportImageView = imageView;
        return reinterpret_cast<ImTextureID>(m_SceneViewportDescriptorSet);
    }

    void ImGuiLayer::ClearSceneViewportTexture()
    {
        if (m_SceneViewportDescriptorSet != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(m_SceneViewportDescriptorSet);
            m_SceneViewportDescriptorSet = VK_NULL_HANDLE;
        }

        m_SceneViewportSampler = VK_NULL_HANDLE;
        m_SceneViewportImageView = VK_NULL_HANDLE;
    }

    float ImGuiLayer::GetToolbarHeight() const
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        const float textHeight = ImGui::GetFontSize();
        const float frameHeight = textHeight + style.FramePadding.y * 2.0f;
        const float contentHeight =
            (frameHeight > ToolbarStyle::ButtonHeight()) ? frameHeight : ToolbarStyle::ButtonHeight();

        return contentHeight + style.WindowPadding.y * 2.0f + Scale(6.0f);
    }

    void ImGuiLayer::CreateDescriptorPool()
    {
        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * static_cast<std::uint32_t>(std::size(poolSizes));
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create ImGui descriptor pool.");
        }
    }

    void ImGuiLayer::DestroyDescriptorPool()
    {
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }
}
