#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

struct ma_engine;

namespace Quiet::Audio
{
    class AudioLibrary;

    class AudioSubsystem
    {
    public:
        AudioSubsystem();
        ~AudioSubsystem();

        AudioSubsystem(const AudioSubsystem&) = delete;
        AudioSubsystem& operator=(const AudioSubsystem&) = delete;
        AudioSubsystem(AudioSubsystem&&) = delete;
        AudioSubsystem& operator=(AudioSubsystem&&) = delete;

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

        [[nodiscard]] AudioLibrary& GetLibrary();
        [[nodiscard]] const AudioLibrary& GetLibrary() const;

        [[nodiscard]] bool PlayOneShot(const std::filesystem::path& filepath) const;
        [[nodiscard]] bool PlayOneShotAsset(std::string_view assetID) const;

    private:
        void Initialize();
        void Shutdown();

    private:
        std::unique_ptr<ma_engine> m_Engine;
        std::unique_ptr<AudioLibrary> m_Library;

        bool m_Initialized = false;
    };
}