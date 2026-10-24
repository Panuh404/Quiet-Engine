#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class Texture2D;

    /**
     * @brief Owns reusable image descriptors and submission texture references.
     * @ingroup quiet_renderer
     *
     * Intended for the renderer's current single in-flight submission.
     * The logical device and supplied descriptor-set layout must outlive
     * this object. GPU use must finish before destruction or BeginFrame().
     */
    class ImageResources
    {
    public:
        ImageResources(const LogicalDevice& logicalDevice, VkDescriptorSetLayout descriptorSetLayout);
        ~ImageResources();

        ImageResources(const ImageResources&) = delete;
        ImageResources& operator=(const ImageResources&) = delete;
        ImageResources(ImageResources&&) = delete;
        ImageResources& operator=(ImageResources&&) = delete;

        /**
         * @brief Retires previous bindings and prepares descriptor capacity.
         * @param requiredSets Upper bound on bindings needed this frame.
         * @pre Previous GPU use of these descriptors and textures has finished.
         * @note Resets existing sets or grows the pool when needed.
         *       Releases previous texture references after retiring descriptors.
         *       Vulkan and allocation failures propagate as exceptions.
         */
        void BeginFrame(std::uint32_t requiredSets);

        /**
         * @brief Creates a distinct descriptor set and retains its texture.
         * @return Descriptor set, or null for a null texture.
         * @pre BeginFrame() prepared sufficient capacity before recording.
         * @note Existing sets are not overwritten.
         *       The texture remains retained until the next BeginFrame()
         *       or destruction, both requiring completed GPU use.
         */
        [[nodiscard]] VkDescriptorSet CreateBinding(std::shared_ptr<Texture2D> texture);

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        std::uint32_t m_Capacity = 0;

        std::vector<std::shared_ptr<Texture2D>> m_RetainedTextures;
    };
}