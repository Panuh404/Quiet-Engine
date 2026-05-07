#include "Renderer/Vulkan/Context/PhysicalDevice.h"

#include "Quiet/Core/Log.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr const char* kRequiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::string FormatApiVersion(std::uint32_t version)
    {
        std::ostringstream out;
        out << VK_API_VERSION_MAJOR(version) << '.'
            << VK_API_VERSION_MINOR(version) << '.'
            << VK_API_VERSION_PATCH(version);
        return out.str();
    }

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
    {
        std::uint32_t count = 0;
        if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to enumerate Vulkan physical devices.");

        if (count == 0)
            throw std::runtime_error("No Vulkan physical devices were found.");

        std::vector<VkPhysicalDevice> devices(count);
        if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to read Vulkan physical devices.");

        return devices;
    }

    Quiet::Renderer::Vulkan::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        Quiet::Renderer::Vulkan::QueueFamilyIndices indices;

        std::uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.GraphicsFamily = i;

            VkBool32 supportsPresent = VK_FALSE;
            if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supportsPresent) != VK_SUCCESS)
                throw std::runtime_error("Failed to query queue family surface support.");

            if (supportsPresent == VK_TRUE) indices.PresentFamily = i;
            if (indices.IsComplete()) break;
        }

        return indices;
    }

    Quiet::Renderer::Vulkan::SwapchainSupportDetails QuerySwapchainSupportLocal(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        Quiet::Renderer::Vulkan::SwapchainSupportDetails details{};

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities) != VK_SUCCESS)
            throw std::runtime_error("Failed to query surface capabilities.");

        std::uint32_t formatCount = 0;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to query surface formats.");

        if (formatCount > 0)
        {
            details.Formats.resize(formatCount);
            if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to read surface formats.");
        }

        std::uint32_t presentModeCount = 0;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to query present modes.");

        if (presentModeCount > 0)
        {
            details.PresentModes.resize(presentModeCount);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to read present modes.");
        }

        return details;
    }

    bool SupportsRequiredExtensions(VkPhysicalDevice device)
    {
        std::uint32_t count = 0;
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to enumerate Vulkan device extensions.");

        std::vector<VkExtensionProperties> extensions(count);
        if (count > 0 && vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to read Vulkan device extensions.");

        std::set<std::string> missing(std::begin(kRequiredDeviceExtensions), std::end(kRequiredDeviceExtensions));
        for (const auto& ext : extensions)
        {
            missing.erase(ext.extensionName);
        }

        return missing.empty();
    }

    int ScoreDevice(const VkPhysicalDeviceProperties& properties, std::uint32_t targetApiVersion)
    {
        int score = 0;

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        if (properties.apiVersion >= targetApiVersion) score += 200;

        score += static_cast<int>(properties.limits.maxImageDimension2D);
        return score;
    }
}

namespace Quiet::Renderer::Vulkan
{
    PhysicalDevice::PhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::uint32_t targetApiVersion)
    {
        const auto devices = EnumeratePhysicalDevices(instance);
        int bestScore = -1;

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);

            const QueueFamilyIndices families = FindQueueFamilies(device, surface);
            const bool hasExtensions = SupportsRequiredExtensions(device);

            SwapchainSupportDetails swapchainSupport{};
            const bool swapchainUsable =
                families.IsComplete() &&
                hasExtensions &&
                [&]()
                {
                    swapchainSupport = QuerySwapchainSupportLocal(device, surface);
                    return !swapchainSupport.Formats.empty() && !swapchainSupport.PresentModes.empty();
                }();

            QUIET_CORE_INFO(
                "GPU candidate: {} | type={} | api={} | graphicsQueue={} | presentQueue={} | swapchainExt={} | formats={} | presentModes={}",
                properties.deviceName,
                static_cast<int>(properties.deviceType),
                FormatApiVersion(properties.apiVersion),
                families.GraphicsFamily.has_value() ? "yes" : "no",
                families.PresentFamily.has_value() ? "yes" : "no",
                hasExtensions ? "yes" : "no",
                swapchainSupport.Formats.size(),
                swapchainSupport.PresentModes.size()
            );

            if (!swapchainUsable) continue;

            const int score = ScoreDevice(properties, targetApiVersion);
            if (score > bestScore)
            {
                bestScore = score;
                m_Device = device;
                m_QueueFamilies = families;
                m_SwapchainSupport = std::move(swapchainSupport);
                m_Properties = properties;
            }
        }

        if (m_Device == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find a suitable Vulkan physical device.");
        }

        vkGetPhysicalDeviceMemoryProperties(m_Device, &m_MemoryProperties);

        QUIET_CORE_INFO(
            "Selected GPU: {} | api={} | graphicsQueueFamily={} | presentQueueFamily={}",
            m_Properties.deviceName,
            FormatApiVersion(m_Properties.apiVersion),
            m_QueueFamilies.GraphicsFamily.value(),
            m_QueueFamilies.PresentFamily.value()
        );
    }

    SwapchainSupportDetails PhysicalDevice::QuerySwapchainSupport(VkSurfaceKHR surface) const
    {
        return QuerySwapchainSupportLocal(m_Device, surface);
    }

    std::uint32_t PhysicalDevice::FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        for (std::uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; ++i)
        {
            const bool typeSupported = (typeFilter & (1u << i)) != 0;
            const bool hasProperties = (m_MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeSupported && hasProperties) return i;
        }
        throw std::runtime_error("Failed to find suitable Vulkan memory type.");
    }
}
