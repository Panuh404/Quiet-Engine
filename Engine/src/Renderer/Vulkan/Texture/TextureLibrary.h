#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Quiet::Renderer::Vulkan
{
    struct TextureStats
    {
        std::string Format;

        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        bool DimensionsValid = false;

        std::uintmax_t FileSizeBytes = 0;

        std::uint32_t WarningCount = 0;
        std::vector<std::string> Warnings;
    };

    class TextureLibrary
    {
    public:
        void RegisterTexture(const std::string& name, TextureStats stats);
        void ReplaceTexture(const std::string& name, TextureStats stats);

        [[nodiscard]] bool Has(const std::string& name) const;
        [[nodiscard]] std::optional<TextureStats> GetStats(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::size_t GetCount() const { return m_Stats.size(); }

        void Clear();

    private:
        std::unordered_map<std::string, TextureStats> m_Stats;
    };
}