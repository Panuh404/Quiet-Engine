#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Audio/AudioAsset.h"

namespace Quiet::Audio
{
    class AudioLibrary
    {
    public:
        [[nodiscard]] bool LoadFromFile(const std::string& assetID, const std::filesystem::path& filePath, bool replaceExisting);

        [[nodiscard]] std::optional<AudioAsset> Get(std::string_view assetID) const;
        [[nodiscard]] bool Has(std::string_view assetID) const;
        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::size_t GetCount() const { return m_Assets.size(); }

        void Clear();

    private:
        std::unordered_map<std::string, AudioAsset> m_Assets;
    };
}