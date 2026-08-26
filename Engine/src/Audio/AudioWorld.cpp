#include "Audio/AudioWorld.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <miniaudio.h>

#include "Audio/AudioLibrary.h"
#include "Audio/AudioSubsystem.h"
#include "Quiet/Core/Log.h"
#include "Scene/Components/ActorIdentityComponent.h"
#include "Scene/Components/AudioSourceComponent.h"

namespace Quiet::Audio
{
    AudioWorld::AudioWorld(AudioSubsystem& audioSubsystem) : m_AudioSubsystem(audioSubsystem)
    {
        QUIET_CORE_INFO("Created audio world.");
    }

    AudioWorld::~AudioWorld()
    {
        DestroySources();
        QUIET_CORE_INFO("Destroyed audio world.");
    }

    void AudioWorld::BuildSources(entt::registry& registry)
    {
        DestroySources();

        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine)
        {
            QUIET_CORE_WARN("Cannot build runtime audio sources: audio subsystem is not initialized.");
            return;
        }

        auto view = registry.view<Scene::ActorIdentityComponent, Scene::AudioSourceComponent>();
        std::size_t startedCount = 0;

        for (const entt::entity entity : view)
        {
            const auto& identity = registry.get<Scene::ActorIdentityComponent>(entity);
            const auto& source = registry.get<Scene::AudioSourceComponent>(entity);

            if (source.AudioAssetID.empty()) continue;
            const auto asset = m_AudioSubsystem.GetLibrary().Get(source.AudioAssetID);

            if (!asset.has_value())
            {
                QUIET_CORE_WARN("Cannot create runtime audio source for actor '{}': audio asset '{}' is not loaded.",
                    Core::UUIDToString(identity.ID), source.AudioAssetID);
                continue;
            }

            auto sound = std::make_unique<ma_sound>();
            const std::string path = asset->SourcePath.string();

            constexpr ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;

            const ma_result initializeResult = ma_sound_init_from_file(m_AudioSubsystem.m_Engine.get(), path.c_str(), flags, nullptr, nullptr, sound.get());
            if (initializeResult != MA_SUCCESS)
            {
                QUIET_CORE_WARN("Failed to initialize runtime audio source '{}' for actor '{}': {}",
                    source.AudioAssetID, Core::UUIDToString(identity.ID), ma_result_description(initializeResult));
                continue;
            }

            const float volume = std::isfinite(source.Volume) ? std::clamp(source.Volume, 0.0f, 4.0f) : 1.0f;
            const float pitch = std::isfinite(source.Pitch) ? std::clamp(source.Pitch, 0.01f, 4.0f) : 1.0f;

            ma_sound_set_volume(sound.get(), volume);
            ma_sound_set_pitch(sound.get(), pitch);
            ma_sound_set_looping(sound.get(), source.Loop ? MA_TRUE : MA_FALSE);

            if (source.PlayOnStart)
            {
                const ma_result startResult = ma_sound_start(sound.get());
                if (startResult != MA_SUCCESS)
                {
                    ma_sound_uninit(sound.get());
                    QUIET_CORE_WARN("Failed to start runtime audio source '{}' for actor '{}': {}",
                        source.AudioAssetID, Core::UUIDToString(identity.ID), ma_result_description(startResult));
                    continue;
                }
                ++startedCount;
            }

            SourceBinding binding{};
            binding.Entity = entity;
            binding.ActorID = identity.ID;
            binding.AudioAssetID = source.AudioAssetID;
            binding.Sound = std::move(sound);

            m_Sources.push_back(std::move(binding));
        }

        QUIET_CORE_INFO("Created {} runtime audio sources. playOnStart={}", m_Sources.size(), startedCount);
    }

    bool AudioWorld::PlaySource(const entt::entity entity)
    {
        SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound) return false;

        // Always restart from the beginning.
        const ma_result stopResult = ma_sound_stop(binding->Sound.get());
        if (stopResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to prepare runtime audio source '{}' for actor '{}' for playback: {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(stopResult));
            return false;
        }

        binding->Paused = false;

        const ma_result seekResult = ma_sound_seek_to_pcm_frame(binding->Sound.get(), 0);
        if (seekResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to rewind runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(seekResult));
            return false;
        }

        const ma_result startResult = ma_sound_start(binding->Sound.get());
        if (startResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to play runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(startResult));
            return false;
        }

        return true;
    }

    bool AudioWorld::StopSource(const entt::entity entity)
    {
        SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound) return false;

        const ma_result stopResult = ma_sound_stop(binding->Sound.get());
        if (stopResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to stop runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(stopResult));
            return false;
        }

        binding->Paused = false;

        // miniaudio stop preserves the cursor.
        // Quiet Stop semantics explicitly rewind it.
        const ma_result seekResult = ma_sound_seek_to_pcm_frame(binding->Sound.get(), 0);
        if (seekResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to rewind stopped runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(seekResult));
            return false;
        }
        return true;
    }

    bool AudioWorld::PauseSource(const entt::entity entity)
    {
        SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound) return false;

        if (ma_sound_is_playing(binding->Sound.get()) != MA_TRUE)
            return false;

        const ma_result result = ma_sound_stop(binding->Sound.get());
        if (result != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to pause runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(result));
            return false;
        }

        binding->Paused = true;
        return true;
    }

    bool AudioWorld::ResumeSource(const entt::entity entity)
    {
        SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound || !binding->Paused)
            return false;

        const ma_result result = ma_sound_start(binding->Sound.get());
        if (result != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to resume runtime audio source '{}' for actor '{}': {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), ma_result_description(result));
            return false;
        }

        binding->Paused = false;
        return true;
    }

    bool AudioWorld::SeekSource(const entt::entity entity, const float seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0.0f)
            return false;

        SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound) return false;

        const ma_result result = ma_sound_seek_to_second(binding->Sound.get(), seconds);
        if (result != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to seek runtime audio source '{}' for actor '{}' to {:.3f}s: {}",
                binding->AudioAssetID, Core::UUIDToString(binding->ActorID), seconds, ma_result_description(result));
            return false;
        }

        return true;
    }

    AudioPlaybackState AudioWorld::GetSourcePlaybackState(const entt::entity entity) const
    {
        const SourceBinding* binding = FindSourceBinding(entity);
        if (!binding || !binding->Sound)
            return AudioPlaybackState::Unavailable;

        if (ma_sound_is_playing(binding->Sound.get()) == MA_TRUE)
            return AudioPlaybackState::Playing;

        if (binding->Paused)
            return AudioPlaybackState::Paused;

        return AudioPlaybackState::Stopped;
    }

    AudioWorld::SourceBinding* AudioWorld::FindSourceBinding(const entt::entity entity)
    {
        for (SourceBinding& binding : m_Sources)
        {
            if (binding.Entity == entity)
                return &binding;
        }

        return nullptr;
    }

    const AudioWorld::SourceBinding* AudioWorld::FindSourceBinding(const entt::entity entity) const
    {
        for (const SourceBinding& binding : m_Sources)
        {
            if (binding.Entity == entity)
                return &binding;
        }

        return nullptr;
    }

    void AudioWorld::DestroySources()
    {
        for (SourceBinding& source : m_Sources)
        {
            if (!source.Sound) continue;
            (void)ma_sound_stop(source.Sound.get());
            ma_sound_uninit(source.Sound.get());
            source.Sound.reset();
        }

        m_Sources.clear();
    }
}