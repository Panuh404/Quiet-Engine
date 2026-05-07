#include "ShaderModule.h"

#include <fstream>
#include <vector>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    ShaderModule::ShaderModule(VkDevice device, const std::string &path)
        : m_Device(device)
    {
        const auto code = ReadSpirvFile(path);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(std::uint32_t);
        createInfo.pCode = code.data();

        if (vkCreateShaderModule(device, &createInfo, nullptr, &m_Module) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module from: " + path);
        }
        QUIET_CORE_INFO("Loaded shader module from '{}'.", path);
    }

    ShaderModule::~ShaderModule()
    {
        if (m_Module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_Module, nullptr);
            m_Module = VK_NULL_HANDLE;
        }
    }

    VkPipelineShaderStageCreateInfo ShaderModule::MakeStageInfo(
        VkShaderStageFlagBits stage,
        const char *entryPoint) const
    {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stage;
        stageInfo.module = m_Module;
        stageInfo.pName = entryPoint;
        return stageInfo;
    }

    std::vector<std::uint32_t> ShaderModule::ReadSpirvFile(const std::string &path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open SPIR-V shader file: " + path);
        }

        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
        {
            throw std::runtime_error("Shader file is empty: " + path);
        }

        if ((fileSize % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0)
        {
            throw std::runtime_error("SPIR-V shader file size is not 4-byte aligned: " + path);
        }

        std::vector<std::uint32_t> code(
            static_cast<std::size_t>(fileSize / static_cast<std::streamsize>(sizeof(std::uint32_t)))
        );

        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(code.data()), fileSize);

        if (!file)
        {
            throw std::runtime_error("Failed to read SPIR-V shader file: " + path);
        }

        return code;
    }
}
