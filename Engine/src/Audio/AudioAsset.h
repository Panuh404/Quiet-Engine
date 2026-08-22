#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Quiet::Audio
{
    struct AudioAsset
    {
        std::filesystem::path SourcePath;

        std::string Encoding;

        std::uint32_t Channels = 0;
        std::uint32_t SampleRate = 0;

        std::uint64_t FrameCount = 0;
        double DurationSeconds = 0.0;
        bool DurationValid = false;

        std::uintmax_t FileSizeBytes = 0;

        [[nodiscard]] bool IsValid() const { return !SourcePath.empty() && Channels > 0 && SampleRate > 0; }
    };
}