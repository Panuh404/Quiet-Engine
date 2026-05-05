#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Quiet::Renderer::Vulkan
{
    struct TextureImportResult
    {
        bool Success = false;
        std::string Error;

        std::string Format;
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        bool DimensionsValid = false;

        std::uintmax_t FileSizeBytes = 0;

        std::vector<std::string> Warnings;

        [[nodiscard]] bool Succeeded() const
        {
            return Success;
        }
    };
}