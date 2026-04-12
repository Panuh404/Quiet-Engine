#include "StaticMeshRenderSystem.h"

#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/CameraResources.h"
#include "Renderer/Vulkan/StaticMesh.h"
#include "Renderer/Vulkan/StaticMeshPipeline.h"
#include "Renderer/Vulkan/RenderTypes.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::Vulkan
{
    void StaticMeshRenderSystem::Record(const StaticMeshRenderContext &context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
        {
            throw std::runtime_error("StaticMeshRenderSystem received null command buffer.");
        }

        if (!context.Pipeline || !context.Camera || !context.SceneData)
        {
            throw std::runtime_error("StaticMeshRenderSystem received invalid render context.");
        }

        vkCmdBindPipeline(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetHandle());

        VkDescriptorSet descriptorSet = context.Camera->GetDescriptorSet(context.ImageIndex);
        vkCmdBindDescriptorSets(
           context.CommandBuffer,
           VK_PIPELINE_BIND_POINT_GRAPHICS,
           context.Pipeline->GetLayout(),
           0,
           1,
           &descriptorSet,
           0,
           nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(context.Extent.width);
        viewport.height = static_cast<float>(context.Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(context.CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = context.Extent;
        vkCmdSetScissor(context.CommandBuffer, 0, 1, &scissor);

        std::uint32_t drawCount = 0;
        std::uint32_t skippedCount = 0;

        for (const auto& actor : context.SceneData->GetActors())
        {
            if (!actor.StaticMesh.has_value() ||
               !actor.StaticMesh->IsValid() ||
               !actor.StaticMesh->Visible)
            {
                ++skippedCount;
                continue;
            }

            const auto& mesh = actor.StaticMesh->Mesh;

            const VkBuffer vertexBuffers[] = { mesh->GetVertexBuffer() };
            const VkDeviceSize offsets[] = { 0 };

            vkCmdBindVertexBuffers(context.CommandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(context.CommandBuffer, mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

            TransformPushConstants push{};
            push.Model = actor.Transform.WorldTransform.ToMatrix();

            vkCmdPushConstants(
                context.CommandBuffer,
                context.Pipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(TransformPushConstants),
                &push
            );

            vkCmdDrawIndexed(
                context.CommandBuffer, mesh->GetIndexCount(),
                1, 0, 0, 0);
            ++drawCount;
        }
        // QUIET_CORE_TRACE(
        //     "StaticMeshRenderSystem: drew {} actors, skipped {}.",
        //     drawCount,
        //     skippedCount
        // );
    }
}
