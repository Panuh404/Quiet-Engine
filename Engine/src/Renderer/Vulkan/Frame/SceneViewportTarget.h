#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;

    class SceneViewportTarget
   {
    public:
        SceneViewportTarget(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkFormat colorFormat,
                            VkFormat depthFormat);
        ~SceneViewportTarget();

        SceneViewportTarget(const SceneViewportTarget&) = delete;
        SceneViewportTarget& operator=(const SceneViewportTarget&) = delete;
        SceneViewportTarget(SceneViewportTarget&&) = delete;
        SceneViewportTarget& operator=(SceneViewportTarget&&) = delete;

        void Resize(std::uint32_t width, std::uint32_t height);
        void Destroy();

        [[nodiscard]] bool IsValid() const { return m_ColorImage != VK_NULL_HANDLE; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_Extent; }

        [[nodiscard]] VkFormat GetColorFormat() const { return m_ColorFormat; }
        [[nodiscard]] VkFormat GetDepthFormat() const { return m_DepthFormat; }

        [[nodiscard]] VkImage GetColorImage() const { return m_ColorImage; }
        [[nodiscard]] VkSampler GetSampler() const { return m_Sampler; }
        [[nodiscard]] VkImageView GetColorImageView() const { return m_ColorImageView; }
        [[nodiscard]] VkImageLayout GetColorLayout() const { return m_ColorLayout; }
        void SetColorLayout(VkImageLayout layout) { m_ColorLayout = layout; }

        [[nodiscard]] VkImage GetDepthImage() const { return m_DepthImage; }
        [[nodiscard]] VkImageView GetDepthImageView() const { return m_DepthImageView; }
        [[nodiscard]] VkImageLayout GetDepthLayout() const { return m_DepthLayout; }
        void SetDepthLayout(VkImageLayout layout) { m_DepthLayout = layout; }

        [[nodiscard]] VkFormat GetSelectionMaskFormat() const { return kSelectionMaskFormat; }
        [[nodiscard]] VkImage GetSelectionMaskImage() const { return m_SelectionMaskImage; }
        [[nodiscard]] VkImageView GetSelectionMaskImageView() const { return m_SelectionMaskImageView; }
        [[nodiscard]] VkImageLayout GetSelectionMaskLayout() const { return m_SelectionMaskLayout; }
        void SetSelectionMaskLayout(VkImageLayout layout) { m_SelectionMaskLayout = layout; }

    private:
        void CreateColorResources(std::uint32_t width, std::uint32_t height);
        void CreateDepthResources(std::uint32_t width, std::uint32_t height);
        void CreateSelectionMaskResources(std::uint32_t width, std::uint32_t height);

        void CreateSampler();

        [[nodiscard]] std::uint32_t FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    private:
        static constexpr VkFormat kSelectionMaskFormat = VK_FORMAT_R8_UNORM;

        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;

        VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};

        VkImage m_ColorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_ColorMemory = VK_NULL_HANDLE;
        VkImageView m_ColorImageView = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;
        VkImageLayout m_ColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage m_DepthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
        VkImageView m_DepthImageView = VK_NULL_HANDLE;
        VkImageLayout m_DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage m_SelectionMaskImage = VK_NULL_HANDLE;
        VkDeviceMemory m_SelectionMaskMemory = VK_NULL_HANDLE;
        VkImageView m_SelectionMaskImageView = VK_NULL_HANDLE;
        VkImageLayout m_SelectionMaskLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
}
