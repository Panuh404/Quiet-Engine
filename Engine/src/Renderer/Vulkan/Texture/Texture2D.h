#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    struct TexturePixelData;

    class Texture2D
    {
    public:
        Texture2D(const PhysicalDevice& physicalDevice,
                  const LogicalDevice& logicalDevice,
                  const GraphicsCommands& graphicsCommands,
                  const TexturePixelData& pixelData);

        ~Texture2D();


        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;
        Texture2D(Texture2D&&) = delete;
        Texture2D& operator=(Texture2D&&) = delete;

        [[nodiscard]] VkImage GetImage() const { return m_Image; }
        [[nodiscard]] VkImageView GetImageView() const { return m_ImageView; }
        [[nodiscard]] VkSampler GetSampler() const { return m_Sampler; }
        [[nodiscard]] VkFormat GetFormat() const { return m_Format; }

        [[nodiscard]] std::uint32_t GetWidth() const { return m_Width; }
        [[nodiscard]] std::uint32_t GetHeight() const { return m_Height; }

    private:
        void Destroy();

        VkDevice m_Device = VK_NULL_HANDLE;
        VkImage m_Image = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;

        VkFormat m_Format = VK_FORMAT_R8G8B8A8_SRGB;

        std::uint32_t m_Width = 0;
        std::uint32_t m_Height = 0;
    };
}