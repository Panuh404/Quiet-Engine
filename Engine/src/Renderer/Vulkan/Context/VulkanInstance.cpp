#include "Renderer/Vulkan/Context/VulkanInstance.h"

#include "Quiet/Core/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
#ifdef NDEBUG
    constexpr bool kEnableValidationLayers = false;
#else
    constexpr bool kEnableValidationLayers = true;
#endif

    constexpr std::array<const char*, 1> kValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    std::string FormatApiVersion(std::uint32_t version)
    {
        std::ostringstream out;
        out << VK_API_VERSION_MAJOR(version) << '.'
            << VK_API_VERSION_MINOR(version) << '.'
            << VK_API_VERSION_PATCH(version);
        return out.str();
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*)
    {
        const char* message = pCallbackData && pCallbackData->pMessage
            ? pCallbackData->pMessage
            : "Unknown Vulkan validation message.";

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) QUIET_CORE_ERROR("[Vulkan] {}", message);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) QUIET_CORE_WARN("[Vulkan] {}", message);
        else QUIET_CORE_INFO("[Vulkan] {}", message);
        return VK_FALSE;
    }

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
        createInfo.pUserData = nullptr;
    }

    std::vector<VkLayerProperties> EnumerateInstanceLayers()
    {
        std::uint32_t count = 0;
        if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to enumerate Vulkan instance layers.");
        }

        std::vector<VkLayerProperties> layers(count);
        if (count > 0 && vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to read Vulkan instance layers.");
        }

        return layers;
    }

    std::vector<VkExtensionProperties> EnumerateInstanceExtensions()
    {
        std::uint32_t count = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to enumerate Vulkan instance extensions.");
        }

        std::vector<VkExtensionProperties> extensions(count);
        if (count > 0 &&
            vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to read Vulkan instance extensions.");
        }

        return extensions;
    }

    bool AreValidationLayersSupported()
    {
        const auto availableLayers = EnumerateInstanceLayers();

        for (const char* requiredLayer : kValidationLayers)
        {
            const bool found = std::any_of(
                availableLayers.begin(),
                availableLayers.end(),
                [requiredLayer](const VkLayerProperties& layer)
                {
                    return std::strcmp(layer.layerName, requiredLayer) == 0;
                });

            if (!found)
            {
                return false;
            }
        }

        return true;
    }

    void ValidateRequiredExtensions(const std::vector<const char*>& requiredExtensions)
    {
        const auto availableExtensions = EnumerateInstanceExtensions();

        for (const char* requiredExtension : requiredExtensions)
        {
            const bool found = std::any_of(
                availableExtensions.begin(),
                availableExtensions.end(),
                [requiredExtension](const VkExtensionProperties& extension)
                {
                    return std::strcmp(extension.extensionName, requiredExtension) == 0;
                });

            if (!found)
            {
                throw std::runtime_error(std::string("Missing required Vulkan instance extension: ") + requiredExtension);
            }
        }
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
        const VkAllocationCallbacks* allocator,
        VkDebugUtilsMessengerEXT* messenger)
    {
        const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

        if (!func) return VK_ERROR_EXTENSION_NOT_PRESENT;
        return func(instance, createInfo, allocator, messenger);
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks* allocator)
    {
        const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

        if (func) func(instance, messenger, allocator);
    }
}

namespace Quiet::Renderer::Vulkan
{
    VulkanInstance::VulkanInstance(const std::string& applicationName, const std::vector<const char*>& windowExtensions)
    {
        m_ValidationEnabled = kEnableValidationLayers;
        if (m_ValidationEnabled && !AreValidationLayersSupported())
        {
            throw std::runtime_error(
                "Validation layers were requested, but VK_LAYER_KHRONOS_validation is not available."
            );
        }

        std::vector<const char*> enabledExtensions = windowExtensions;
        if (m_ValidationEnabled)
        {
            enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        ValidateRequiredExtensions(enabledExtensions);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = applicationName.c_str();
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.pEngineName = "Quiet";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.apiVersion = m_ApiVersion;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (m_ValidationEnabled)
        {
            createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance.");
        }

        if (m_ValidationEnabled)
        {
            VkDebugUtilsMessengerCreateInfoEXT persistentCreateInfo{};
            PopulateDebugMessengerCreateInfo(persistentCreateInfo);

            if (CreateDebugUtilsMessengerEXT(
                    m_Instance, &persistentCreateInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Vulkan debug messenger.");
            }
        }

        QUIET_CORE_INFO("Created Vulkan instance. Target API version: {}", FormatApiVersion(m_ApiVersion));
        QUIET_CORE_INFO("Validation layers: {}", m_ValidationEnabled ? "enabled" : "disabled");
    }

    VulkanInstance::~VulkanInstance()
    {
        if (m_DebugMessenger != VK_NULL_HANDLE)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
            m_DebugMessenger = VK_NULL_HANDLE;
        }

        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }
    }
}
