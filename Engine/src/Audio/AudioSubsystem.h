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

    /**
     * @brief Owns shared audio playback infrastructure and asset metadata.
     * @ingroup quiet_audio
     *
     * Owns the miniaudio engine, audio library, preview sound, and mixing groups.
     * Must outlive AudioWorld instances that borrow its playback infrastructure.
     */
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

        /**
         * @brief Sets a shared bus level, clamping finite input to 0–4.
         * @return False for invalid input, unavailable infrastructure,
         *         or a reported backend failure.
         */
        bool SetBusVolume(AudioBus bus, float volume);

        /** @brief Returns the stored bus level, or 1 for an invalid bus. */
        [[nodiscard]] float GetBusVolume(AudioBus bus) const;

        /**
         * @brief Requests one-shot file playback without a dedicated sound group.
         * @return True when the backend request succeeds.
         * @note Returns no playback control handle.
         */
        [[nodiscard]] bool PlayOneShot(const std::filesystem::path& filepath) const;

        /** @brief Resolves loaded metadata and requests one-shot playback. */
        [[nodiscard]] bool PlayOneShotAsset(std::string_view assetID) const;

        /**
         * @brief Replaces the preview with a streamed, nonspatial sound.
         * @return True when initialization and playback startup succeed.
         * @note Missing metadata preserves the current preview.
         * @note Replacement failure after metadata resolution leaves no preview.
         */
        [[nodiscard]] bool StartPreviewAsset(std::string_view assetID);

        /** @brief Releases the preview sound and clears its recorded asset ID. */
        void StopPreview();

        /** @brief Reports whether the preview sound is playing. */
        [[nodiscard]] bool IsPreviewPlaying() const;

        /** @brief Reports whether the requested asset is the playing preview. */
        [[nodiscard]] bool IsPreviewingAsset(std::string_view assetID) const;

        /**
         * @brief Returns the recorded preview ID, which may remain after completion.
         * @note Borrows storage that preview changes can invalidate.
         */
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