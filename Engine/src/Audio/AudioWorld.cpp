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
#include "Scene/Components/TransformComponent.h"

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

    void AudioWorld::BuildSources(entt::registry& registry, const entt::entity listenerEntity)
    {
        DestroySources();

        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine)
        {
            QUIET_CORE_WARN("Cannot build runtime audio sources: audio subsystem is not initialized.");
            return;
        }

        // Establish the runtime listener before PlayOnStart sources can begin.
        UpdateListener(registry, listenerEntity);

        auto view = registry.view<Scene::ActorIdentityComponent, Scene::AudioSourceComponent>();
        std::size_t startedCount = 0;
        std::size_t spatialCount = 0;

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

            ma_uint32 flags = MA_SOUND_FLAG_STREAM;
            if (!source.Spatial)
            {
                flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
            }

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

            if (source.Spatial)
            {
                const auto& transform = registry.get<Scene::TransformComponent>(entity).WorldTransform;
                ma_sound_set_position(sound.get(), transform.Position.x, transform.Position.y, transform.Position.z);

                const float minDistance = std::isfinite(source.MinDistance) ? std::clamp(source.MinDistance, 0.01f, 99999.99f) : 1.0f;
                const float maxDistance = std::isfinite(source.MaxDistance) ? std::clamp(source.MaxDistance, minDistance + 0.01f, 100000.0f) : 100000.0f;
                const float rolloff = std::isfinite(source.Rolloff) ? std::clamp(source.Rolloff, 0.0f, 10.0f) : 1.0f;

                ma_sound_set_attenuation_model(sound.get(), ma_attenuation_model_inverse);
                ma_sound_set_min_distance(sound.get(), minDistance);
                ma_sound_set_max_distance(sound.get(), maxDistance);
                ma_sound_set_rolloff(sound.get(), rolloff);

                const float coneInnerAngleDegrees = std::isfinite(source.ConeInnerAngleDegrees) ? std::clamp(source.ConeInnerAngleDegrees, 0.0f, 360.0f) : 360.0f;
                const float coneOuterAngleDegrees = std::isfinite(source.ConeOuterAngleDegrees) ? std::clamp(source.ConeOuterAngleDegrees, coneInnerAngleDegrees, 360.0f) : 360.0f;
                const float coneOuterGain = std::isfinite(source.ConeOuterGain) ? std::clamp(source.ConeOuterGain, 0.0f, 1.0f) : 0.0f;

                glm::mat4 sourceRotation(1.0f);
                sourceRotation = glm::rotate(sourceRotation, glm::radians(transform.RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
                sourceRotation = glm::rotate(sourceRotation, glm::radians(transform.RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
                sourceRotation = glm::rotate(sourceRotation, glm::radians(transform.RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));

                const glm::vec3 sourceForward = glm::normalize(glm::vec3(sourceRotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                ma_sound_set_direction(sound.get(), sourceForward.x, sourceForward.y, sourceForward.z);
                ma_sound_set_cone(sound.get(), glm::radians(coneInnerAngleDegrees), glm::radians(coneOuterAngleDegrees), coneOuterGain);

                // Velocity synchronization is intentionally deferred.
                // Prevent Doppler from becoming part of this step.
                ma_sound_set_doppler_factor(sound.get(), 0.0f);

                ++spatialCount;
            }

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
            binding.Spatial = source.Spatial;

            m_Sources.push_back(std::move(binding));
        }

        QUIET_CORE_INFO(
            "Created {} runtime audio sources. spatial={}, playOnStart={}",
            m_Sources.size(),
            spatialCount,
            startedCount
        );
    }

    void AudioWorld::Update(entt::registry &registry, entt::entity listenerEntity)
    {
        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine) return;
        UpdateListener(registry, listenerEntity);

        for (SourceBinding& binding : m_Sources)
        {
            if (!binding.Sound || !binding.Spatial) continue;
            if (!registry.valid(binding.Entity)) continue;

            const auto* transformComponent = registry.try_get<Scene::TransformComponent>(binding.Entity);
            if (!transformComponent) continue;
            const auto& transform = transformComponent->WorldTransform;
            ma_sound_set_position(binding.Sound.get(), transform.Position.x, transform.Position.y, transform.Position.z);

            glm::mat4 rotation(1.0f);
            rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
            rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));

            const glm::vec3 forward = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            ma_sound_set_direction(binding.Sound.get(), forward.x, forward.y, forward.z);
        }
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

    void AudioWorld::UpdateListener(entt::registry &registry, entt::entity listenerEntity)
    {
        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine) return;

        constexpr ma_uint32 listenerIndex = 0;
        if (listenerEntity == entt::null || !registry.valid(listenerEntity))
        {
            ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_FALSE);
            return;
        }

        const auto* transformComponent = registry.try_get<Scene::TransformComponent>(listenerEntity);
        if (!transformComponent)
        {
            ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_FALSE);
            return;
        }

        const auto& transform = transformComponent->WorldTransform;
        glm::mat4 rotation(1.0f);

        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));

        const glm::vec3 forward = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        const glm::vec3 up = glm::normalize(glm::vec3( rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));

        ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_TRUE);
        ma_engine_listener_set_position(m_AudioSubsystem.m_Engine.get(), listenerIndex, transform.Position.x, transform.Position.y, transform.Position.z);
        ma_engine_listener_set_direction(m_AudioSubsystem.m_Engine.get(), listenerIndex, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(m_AudioSubsystem.m_Engine.get(), listenerIndex, up.x, up.y, up.z);
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
