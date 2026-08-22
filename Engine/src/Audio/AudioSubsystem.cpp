#include "Audio/AudioSubsystem.h"

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

    void AudioSubsystem::Initialize()
    {
        if (m_Initialized) return;

        m_Engine = std::make_unique<ma_engine>();
        ma_engine_config config = ma_engine_config_init();

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
