#include "Renderer/Vulkan/Editor/EditorGridRenderSystem.h"

#include <stdexcept>

#include "Renderer/Vulkan/Editor/EditorGridPipeline.h"
#include "Renderer/Vulkan/Frame/CameraResources.h"

namespace Quiet::Renderer::Vulkan
{
    void EditorGridRenderSystem::Record(const EditorGridRenderContext& context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
            throw std::runtime_error("EditorGridRenderSystem received a null command buffer.");

        if (!context.Pipeline || !context.Camera)
            throw std::runtime_error("EditorGridRenderSystem received an invalid render context.");

        if (context.Extent.width == 0 || context.Extent.height == 0)
            return;

        vkCmdBindPipeline(context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.Pipeline->GetHandle());
        const VkDescriptorSet cameraDescriptor = context.Camera->GetDescriptorSet(context.ImageIndex);

        vkCmdBindDescriptorSets(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetLayout(),
            0,
            1,
            &cameraDescriptor,
            0,
            nullptr
        );

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(context.Extent.width);
        viewport.height = static_cast<float>(context.Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(context.CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = context.Extent;
        vkCmdSetScissor(context.CommandBuffer, 0, 1, &scissor);

        EditorGridPushConstants push{};
        push.MinorColor = glm::vec4(0.30f, 0.34f, 0.42f, 0.22f);
        push.MajorColor = glm::vec4(0.48f, 0.53f, 0.62f, 0.42f);
        push.XAxisColor = glm::vec4(0.85f, 0.24f, 0.24f, 0.88f);
        push.ZAxisColor = glm::vec4(0.20f, 0.44f, 0.92f, 0.88f);
        // Base spacing starts at one world unit. Each camera-height
        // decade promotes the next grid level:
        // 1/10, 10/100, 100/1000, and so on.
        push.Parameters = glm::vec4(1.0f, 10.0f, 35.0f, 140.0f);

        vkCmdPushConstants(context.CommandBuffer, context.Pipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(EditorGridPushConstants), &push);
        vkCmdDraw(context.CommandBuffer, 3, 1, 0, 0);
    }
}