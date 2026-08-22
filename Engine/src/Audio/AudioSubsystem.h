#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct ma_engine;
struct ma_sound;

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

        [[nodiscard]] bool StartPreviewAsset(std::string_view assetID);
        void StopPreview();

        [[nodiscard]] bool IsPreviewPlaying() const;
        [[nodiscard]] bool IsPreviewingAsset(std::string_view assetID) const;

        [[nodiscard]] std::string_view GetPreviewAssetID() const { return m_PreviewAssetID; }

    private:
        void Initialize();
        void Shutdown();

    private:
        std::unique_ptr<ma_engine> m_Engine;
        std::unique_ptr<AudioLibrary> m_Library;
        std::unique_ptr<ma_sound> m_PreviewSound;

        std::string m_PreviewAssetID;

        bool m_Initialized = false;
    };
}