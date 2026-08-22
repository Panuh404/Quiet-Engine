#include "Audio/AudioSubsystem.h"
#include "Audio/AudioLibrary.h"

#include <stdexcept>
#include <string>

#include <miniaudio.h>

#include <Quiet/Core/Log.h>

namespace Quiet::Audio
{
    AudioSubsystem::AudioSubsystem()
    {
        Initialize();
    }

    AudioSubsystem::~AudioSubsystem()
    {
        Shutdown();
    }

    AudioLibrary & AudioSubsystem::GetLibrary()
    {
        return *m_Library;
    }

    const AudioLibrary & AudioSubsystem::GetLibrary() const
    {
        return *m_Library;
    }

    bool AudioSubsystem::PlayOneShot(const std::filesystem::path &filepath) const
    {
        if (!m_Initialized || !m_Engine || filepath.empty()) return false;

        const std::string path = filepath.string();
        const ma_result result = ma_engine_play_sound(m_Engine.get(), path.c_str(), nullptr);

        if (result != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to play audio '{}': {}", path, ma_result_description(result));
            return false;
        }

        QUIET_CORE_INFO("Playing one-shot audio '{}'.", path);
        return true;
    }

    bool AudioSubsystem::PlayOneShotAsset(const std::string_view assetID) const
    {
        if (!m_Library) return false;
        const auto asset = m_Library->Get(assetID);

        if (!asset.has_value())
        {
            QUIET_CORE_WARN("Cannot play audio asset '{}': asset is not loaded.", assetID);
            return false;
        }

        return PlayOneShot(asset->SourcePath);
    }

    bool AudioSubsystem::StartPreviewAsset(
        const std::string_view assetID)
    {
        if (!m_Initialized || !m_Engine || !m_Library) return false;

        const auto asset = m_Library->Get(assetID);
        if (!asset.has_value())
        {
            QUIET_CORE_WARN("Cannot preview audio asset '{}': asset metadata is not loaded.", assetID);
            return false;
        }

        StopPreview();

        auto previewSound = std::make_unique<ma_sound>();
        const std::string path = asset->SourcePath.string();
        constexpr ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

        const ma_result initializeResult =
            ma_sound_init_from_file(
                m_Engine.get(),
                path.c_str(),
                flags,
                nullptr,
                nullptr,
                previewSound.get());

        if (initializeResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to initialize preview for audio asset '{}': {}", assetID, ma_result_description(initializeResult));
            return false;
        }

        const ma_result startResult = ma_sound_start(previewSound.get());
        if (startResult != MA_SUCCESS)
        {
            ma_sound_uninit(previewSound.get());
            QUIET_CORE_WARN("Failed to start preview for audio asset '{}': {}", assetID, ma_result_description(startResult));
            return false;
        }

        m_PreviewSound = std::move(previewSound);
        m_PreviewAssetID = std::string(assetID);

        QUIET_CORE_INFO("Started audio preview '{}'.", m_PreviewAssetID);
        return true;
    }

    void AudioSubsystem::StopPreview()
    {
        if (!m_PreviewSound)
        {
            m_PreviewAssetID.clear();
            return;
        }

        ma_sound_stop(m_PreviewSound.get());
        ma_sound_uninit(m_PreviewSound.get());

        m_PreviewSound.reset();
        if (!m_PreviewAssetID.empty())
        {
            QUIET_CORE_INFO("Stopped audio preview '{}'.", m_PreviewAssetID);
        }
        m_PreviewAssetID.clear();
    }

    bool AudioSubsystem::IsPreviewPlaying() const
    {
        return m_PreviewSound && ma_sound_is_playing(m_PreviewSound.get()) == MA_TRUE;
    }

    bool AudioSubsystem::IsPreviewingAsset(std::string_view assetID) const
    {
        return IsPreviewPlaying() && m_PreviewAssetID == assetID;
    }

    void AudioSubsystem::Initialize()
    {
        if (m_Initialized) return;

        m_Engine = std::make_unique<ma_engine>();
        ma_engine_config config = ma_engine_config_init();
        m_Library = std::make_unique<AudioLibrary>();

        const ma_result result = ma_engine_init(&config, m_Engine.get());
        if (result != MA_SUCCESS)
        {
            const std::string error = ma_result_description(result);
            m_Engine.reset();
            throw std::runtime_error("Failed to initialize miniaudio: " + error);
        }

        m_Initialized = true;

        ma_device* device = ma_engine_get_device(m_Engine.get());
        const char* backendName = "Unknown";
        char deviceName[MA_MAX_DEVICE_NAME_LENGTH + 1]{};

        if (device && device->pContext)
        {
            backendName = ma_get_backend_name(device->pContext->backend);
            ma_device_get_name(device, ma_device_type_playback, deviceName, sizeof(deviceName), nullptr);
        }

        QUIET_CORE_INFO("Initialized miniaudio subsystem. backend={}, device='{}', sampleRate={}Hz, channels={}",
            backendName,
            deviceName[0] != '\0' ? deviceName : "<unknown>",
            ma_engine_get_sample_rate(m_Engine.get()),
            ma_engine_get_channels(m_Engine.get()));
    }

    void AudioSubsystem::Shutdown()
    {
        StopPreview();
        m_Library.reset();

        if (!m_Engine)
        {
            m_Initialized = false;
            return;
        }

        if (m_Initialized)
        {
            ma_engine_uninit(m_Engine.get());
        }

        m_Engine.reset();
        m_Initialized = false;

        QUIET_CORE_INFO("Shut down miniaudio audio subsystem.");
    }

}
