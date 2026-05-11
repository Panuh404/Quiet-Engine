#include "Renderer/Vulkan/Mesh/StaticMeshRenderSystem.h"

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "glm/vec4.hpp"

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/Mesh/StaticMeshPipeline.h"
#include "Renderer/Vulkan/Frame/CameraResources.h"
#include "Renderer/Vulkan/RenderTypes.h"
#include "Renderer/Vulkan/Mesh/StaticMeshMaterialResources.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"

#include "Scene/Actor.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderSettings.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Components/MaterialComponent.h"

namespace
{
    const Quiet::Scene::DirectionalLightComponent* FindActiveDirectionalLight(const Quiet::Scene::Scene& scene)
    {
        for (const Quiet::Scene::Actor& actor : scene.GetActors())
        {
            if (actor.DirectionalLight.has_value() && actor.DirectionalLight->Enabled)
                return &actor.DirectionalLight.value();
        }
        return nullptr;
    }

    bool IsValidSubmeshDrawRange(const Quiet::Renderer::Vulkan::StaticSubmesh& submesh,
        const Quiet::Renderer::Vulkan::StaticMesh& mesh)
    {
        if (submesh.IndexCount == 0) return false;
        if (submesh.BaseIndex >= mesh.GetIndexCount()) return false;

        const std::uint64_t endIndex =
            static_cast<std::uint64_t>(submesh.BaseIndex) +
            static_cast<std::uint64_t>(submesh.IndexCount);

        return endIndex <= mesh.GetIndexCount();
    }

    const Quiet::Scene::MaterialComponent* ResolveSubmeshMaterial(const Quiet::Scene::Actor& actor,
        const Quiet::Renderer::Vulkan::StaticSubmesh& submesh)
    {
        (void)submesh;
        if (!actor.Material.has_value()) return nullptr;
        return &actor.Material.value();
    }

    void ApplyMaterialToPushConstants(const Quiet::Scene::MaterialComponent* material,
        Quiet::Renderer::Vulkan::StaticMeshPushConstants& push)
    {
        if (!material) return;
        push.MaterialBaseColor = material->BaseColor;
    }

    VkDescriptorSet ResolveMaterialDescriptorSet(const Quiet::Scene::MaterialComponent* material,
        Quiet::Renderer::Vulkan::StaticMeshMaterialResources& materialResources,
        const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary)
    {
        if (!material || !material->HasBaseColorTexture())
            return materialResources.GetFallbackDescriptorSet();

        return materialResources.GetDescriptorSetForTexture(
            textureLibrary,
            material->BaseColorTextureAssetID
        );
    }

    void DrawSubmesh(const Quiet::Renderer::Vulkan::StaticMeshRenderContext &context,
                     const Quiet::Scene::Actor &actor,
                     const Quiet::Renderer::Vulkan::StaticSubmesh &submesh,
                     Quiet::Renderer::Vulkan::StaticMeshPushConstants push,
                     uint32_t &drawCount)
    {
        const Quiet::Scene::MaterialComponent* material = ResolveSubmeshMaterial(actor, submesh);
        ApplyMaterialToPushConstants(material, push);

        const VkDescriptorSet materialDescriptorSet =
            ResolveMaterialDescriptorSet(
                material,
                *context.Materials,
                *context.Textures
            );

        vkCmdBindDescriptorSets(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetLayout(),
            1,
            1,
            &materialDescriptorSet,
            0,
            nullptr
        );

        vkCmdPushConstants(
            context.CommandBuffer,
            context.Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Quiet::Renderer::Vulkan::StaticMeshPushConstants),
            &push
        );

        vkCmdDrawIndexed(
            context.CommandBuffer,
            submesh.IndexCount,
            1,
            submesh.BaseIndex,
            0,
            0
        );
        ++drawCount;
    }

    void DrawWholeMesh(const Quiet::Renderer::Vulkan::StaticMeshRenderContext& context,
        const Quiet::Scene::Actor& actor,
        const Quiet::Renderer::Vulkan::StaticMesh& mesh,
        Quiet::Renderer::Vulkan::StaticMeshPushConstants push,
        std::uint32_t& drawCount)
    {
        const Quiet::Scene::MaterialComponent* material =
            actor.Material.has_value()
                ? &actor.Material.value()
                : nullptr;

        ApplyMaterialToPushConstants(material, push);

        const VkDescriptorSet materialDescriptorSet =
            ResolveMaterialDescriptorSet(
                material,
                *context.Materials,
                *context.Textures
            );

        vkCmdBindDescriptorSets(
            context.CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.Pipeline->GetLayout(),
            1,
            1,
            &materialDescriptorSet,
            0,
            nullptr
        );

        vkCmdPushConstants(
            context.CommandBuffer,
            context.Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Quiet::Renderer::Vulkan::StaticMeshPushConstants),
            &push
        );

        vkCmdDrawIndexed(
            context.CommandBuffer,
            mesh.GetIndexCount(),
            1,
            0,
            0,
            0
        );
        ++drawCount;
    }
}

namespace Quiet::Renderer::Vulkan
{
    void StaticMeshRenderSystem::Record(const StaticMeshRenderContext &context) const
    {
        if (context.CommandBuffer == VK_NULL_HANDLE)
            throw std::runtime_error("StaticMeshRenderSystem received null command buffer.");

        if (!context.Pipeline ||
            !context.Camera ||
            !context.Materials ||
            !context.Textures ||
            !context.SceneData)
            throw std::runtime_error("StaticMeshRenderSystem received invalid render context.");

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

        const Scene::SceneRenderSettings& renderSettings = context.SceneData->GetRenderSettings();
        const Scene::DirectionalLightComponent* activeLight = FindActiveDirectionalLight(*context.SceneData);

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
            vkCmdBindIndexBuffer(context.CommandBuffer, mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            StaticMeshPushConstants push{};
            push.Model = actor.Transform.WorldTransform.ToMatrix();
            if (activeLight)
            {
                push.LightDirectionIntensity = glm::vec4(
                    activeLight->Direction,
                    activeLight->Intensity
                );
                push.LightColor = glm::vec4(
                    activeLight->Color,
                    1.0f
                );
            }
            else
            {
                push.LightDirectionIntensity = glm::vec4(
                    renderSettings.LightDirection,
                    renderSettings.LightIntensity
                );
                push.LightColor = glm::vec4(1.0f);
            }

            push.AmbientColorIntensity = glm::vec4(
                renderSettings.AmbientColor,
                renderSettings.AmbientIntensity
            );

            const std::vector<StaticSubmesh>& submeshes = mesh->GetSubmeshes();

            if (!submeshes.empty())
            {
                for (const StaticSubmesh& submesh : submeshes)
                {
                    if (!IsValidSubmeshDrawRange(submesh, *mesh))
                    {
                        QUIET_CORE_WARN(
                            "Skipped invalid submesh draw range. mesh='{}' submesh='{}' baseIndex={} indexCount={} meshIndexCount={}",
                            actor.StaticMesh->MeshResourceID,
                            submesh.Name,
                            submesh.BaseIndex,
                            submesh.IndexCount,
                            mesh->GetIndexCount()
                        );
                        continue;
                    }

                    DrawSubmesh(context, actor, submesh, push, drawCount);
                }
            }
            else
            {
                DrawWholeMesh(context, actor, *mesh, push, drawCount);
            }
        }
    }
}
