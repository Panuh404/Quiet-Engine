#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    struct QueueFamilyIndices
    {
        std::optional<std::uint32_t> GraphicsFamily;
        std::optional<std::uint32_t> PresentFamily;

        bool IsComplete() const
        {
            return GraphicsFamily.has_value() && PresentFamily.has_value();
        }
    };

    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class PhysicalDevice
    {
    public:
        PhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::uint32_t targetApiVersion);

        [[nodiscard]] SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface) const;
        [[nodiscard]] std::uint32_t FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

        [[nodiscard]] VkPhysicalDevice GetHandle() const { return m_Device; }
        [[nodiscard]] const QueueFamilyIndices& GetQueueFamilies() const { return m_QueueFamilies; }
        [[nodiscard]] const SwapchainSupportDetails& GetSwapchainSupport() const { return m_SwapchainSupport; }
        [[nodiscard]] const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }

    private:
        VkPhysicalDevice m_Device = VK_NULL_HANDLE;
        QueueFamilyIndices m_QueueFamilies ;
        SwapchainSupportDetails m_SwapchainSupport;
        VkPhysicalDeviceProperties m_Properties{};
        VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
    };
}