#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <Audio/AudioBus.h>

struct ma_engine;
struct ma_sound;
using ma_sound_group = ma_sound;

namespace Quiet::Audio
{
    class AudioLibrary;
    class AudioWorld;

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

        bool SetBusVolume(AudioBus bus, float volume);
        [[nodiscard]] float GetBusVolume(AudioBus bus) const;

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

        [[nodiscard]] ma_sound_group* GetSoundGroup(AudioBus bus) const;
        void ShutdownSoundGroups();

    private:
        std::unique_ptr<ma_engine> m_Engine;
        std::unique_ptr<AudioLibrary> m_Library;
        std::unique_ptr<ma_sound> m_PreviewSound;

        std::string m_PreviewAssetID;
        bool m_Initialized = false;

        std::unique_ptr<ma_sound_group> m_MusicGroup;
        std::unique_ptr<ma_sound_group> m_SfxGroup;
        std::unique_ptr<ma_sound_group> m_UIGroup;

        float m_MasterVolume = 1.0f;
        float m_MusicVolume = 1.0f;
        float m_SfxVolume = 1.0f;
        float m_UIVolume = 1.0f;

        friend class AudioWorld;
    };
}