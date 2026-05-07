#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class VulkanInstance
    {
    public:
        VulkanInstance(const std::string& applicationName,
                       const std::vector<const char*>& windowExtensions);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;
        VulkanInstance(VulkanInstance&&) = delete;
        VulkanInstance& operator=(VulkanInstance&&) = delete;

        VkInstance GetHandle() const { return m_Instance; }
        std::uint32_t GetApiVersion() const { return m_ApiVersion; }

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        std::uint32_t m_ApiVersion = VK_API_VERSION_1_3;
        bool m_ValidationEnabled = false;
    };
}