#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Context/PhysicalDevice.h"
#include "Quiet/Core/Log.h"

#include <set>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr const char* kRequiredDeviceExtensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

namespace Quiet::Renderer::Vulkan
{
    LogicalDevice::LogicalDevice(const PhysicalDevice &physicalDevice)
    {
        const auto& queueFamilies = physicalDevice.GetQueueFamilies();

        std::set<std::uint32_t> uniqueQueueFamilies = {
            queueFamilies.GraphicsFamily.value(),
            queueFamilies.PresentFamily.value(),
        };

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());

        for (std::uint32_t familyIndex : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = familyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfo.flags = 0;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

        VkPhysicalDeviceFeatures2 supportedFeatures{};
        supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        supportedFeatures.pNext = &dynamicRenderingFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice.GetHandle(), &supportedFeatures);

        if (dynamicRenderingFeatures.dynamicRendering != VK_TRUE)
        {
            throw std::runtime_error("Selected GPU does not support dynamic rendering.");
        }

        VkPhysicalDeviceDynamicRenderingFeatures enabledDynamicRendering{};
        enabledDynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        enabledDynamicRendering.dynamicRendering = VK_TRUE;

        VkPhysicalDeviceFeatures2 enabledFeatures{};
        enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        enabledFeatures.pNext = &enabledDynamicRendering;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(std::size(kRequiredDeviceExtensions));
        createInfo.ppEnabledExtensionNames = kRequiredDeviceExtensions;
        createInfo.pNext = &enabledFeatures;
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;

        if (vkCreateDevice(physicalDevice.GetHandle(), &createInfo, nullptr, &m_Device) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan logical device.");

        vkGetDeviceQueue(m_Device, queueFamilies.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, queueFamilies.PresentFamily.value(), 0, &m_PresentQueue);

        QUIET_CORE_INFO(
            "Created logical device. graphicsQueueFamily={} presentQueueFamily={}",
            queueFamilies.GraphicsFamily.value(),
            queueFamilies.PresentFamily.value()
        );
    }

    LogicalDevice::~LogicalDevice()
    {
        if (m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
            m_GraphicsQueue = VK_NULL_HANDLE;
            m_PresentQueue = VK_NULL_HANDLE;
        }
    }
}
