#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Quiet/Assets/QTextureFormat.h"

namespace Quiet::Assets
{
    struct QTextureData
    {
        QTexturePayloadFormat Format = QTexturePayloadFormat::Unknown;
        std::vector<std::uint8_t> EncodedData;
    };

    class QTextureReader
    {
    public:
        static bool LoadFromFile(const std::filesystem::path& path, QTextureData& outTexture, std::string& outError);
    };
}