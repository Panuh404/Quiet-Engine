#include "FrameSync.h"

#include <stdexcept>

#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/Swapchain.h"

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    FrameSync::FrameSync(const LogicalDevice& logicalDevice, const Swapchain& swapchain)
      : m_Device(logicalDevice.GetHandle())
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image-available semaphore.");
        }

        m_RenderFinishedSemaphores.resize(swapchain.GetImages().size(), VK_NULL_HANDLE);

        try
        {
            for (VkSemaphore& semaphore : m_RenderFinishedSemaphores)
            {
                if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to create render-finished semaphore.");
                }
            }

            if (vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create in-flight fence.");
            }
        }
        catch (...)
        {
            for (VkSemaphore semaphore : m_RenderFinishedSemaphores)
            {
                if (semaphore != VK_NULL_HANDLE)
                {
                    vkDestroySemaphore(m_Device, semaphore, nullptr);
                }
            }
            m_RenderFinishedSemaphores.clear();

            if (m_ImageAvailableSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphore, nullptr);
                m_ImageAvailableSemaphore = VK_NULL_HANDLE;
            }

            throw;
        }
    }

    FrameSync::~FrameSync()
    {
        if (m_InFlightFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, m_InFlightFence, nullptr);
            m_InFlightFence = VK_NULL_HANDLE;
        }

        for (VkSemaphore semaphore : m_RenderFinishedSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, semaphore, nullptr);
            }
        }
        m_RenderFinishedSemaphores.clear();

        if (m_ImageAvailableSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, m_ImageAvailableSemaphore, nullptr);
            m_ImageAvailableSemaphore = VK_NULL_HANDLE;
        }
    }
}
