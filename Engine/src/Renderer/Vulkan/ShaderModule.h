#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class ShaderModule
    {
    public:
        ShaderModule(VkDevice device, const std::string& path);
        ~ShaderModule();

        ShaderModule(const ShaderModule&) = delete;
        ShaderModule& operator=(const ShaderModule&) = delete;
        ShaderModule(ShaderModule&&) = delete;
        ShaderModule& operator=(ShaderModule&&) = delete;

        [[nodiscard]] VkShaderModule GetHandle() const { return m_Module; }
        [[nodiscard]] VkPipelineShaderStageCreateInfo MakeStageInfo(
            VkShaderStageFlagBits stage,
            const char* entryPoint = "main") const;

    private:
        static std::vector<std::uint32_t> ReadSpirvFile(const std::string& path);

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkShaderModule m_Module = VK_NULL_HANDLE;
    };
}