#include "Renderer/Vulkan/Texture/TextureLibrary.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    void TextureLibrary::RegisterTexture(const std::string &name, TextureStats stats)
    {
        if (Has(name))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", name);
            throw std::runtime_error("Texture already exists in TextureLibrary: " + name);
        }

        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Registered texture metadata '{}'.", name);
    }

    void TextureLibrary::ReplaceTexture(const std::string &name, TextureStats stats)
    {
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Replaced texture metadata '{}'.", name);
    }

    bool TextureLibrary::Has(const std::string &name) const
    {
        return m_Stats.contains(name);
    }

    std::optional<TextureStats> TextureLibrary::GetStats(const std::string &name) const
    {
        const auto it = m_Stats.find(name);
        if (it == m_Stats.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<std::string> TextureLibrary::GetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Stats.size());

        for (const auto& [name, stats] : m_Stats)
        {
            (void)stats;
            names.push_back(name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    void TextureLibrary::Clear()
    {
        m_Stats.clear();
    }
}
