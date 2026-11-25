#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Audio/AudioAsset.h"

#include "Quiet/Core/UUID.h"

namespace Quiet::Audio
{
    class AudioLibrary
    {
    public:
        [[nodiscard]] bool LoadFromFile(Core::UUID assetID, const std::filesystem::path& filePath, bool replaceExisting);

        [[nodiscard]] std::optional<AudioAsset> Get(Core::UUID assetID) const;
        [[nodiscard]] bool Has(Core::UUID assetID) const;
        [[nodiscard]] std::vector<Core::UUID> GetAssetIDs() const;
        [[nodiscard]] std::size_t GetCount() const { return m_Audios.size(); }

        void Clear();

    private:
        std::unordered_map<Core::UUID, AudioAsset> m_Audios;
    };
}