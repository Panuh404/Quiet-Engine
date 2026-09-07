#include "Audio/AudioWorld.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <miniaudio.h>

#include "Audio/AudioLibrary.h"
#include "Audio/AudioSubsystem.h"
#include "Quiet/Core/Log.h"
#include "Scene/SceneAudioSettings.h"
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
        RestoreMixSettings();
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

        // Establish the runtime listener before PlayOnStart sources begin.
        UpdateListener(registry, listenerEntity, 0.0f);

        auto view = registry.view<Scene::ActorIdentityComponent, Scene::TransformComponent, Scene::AudioSourceComponent>();

        std::size_t startedCount = 0;
        std::size_t spatialCount = 0;

        for (const entt::entity entity : view)
        {
            const auto& source = registry.get<Scene::AudioSourceComponent>(entity);

            if (source.AudioAssetID.empty()) continue;
            if (!CreateSourceBinding(registry, entity)) continue;

            if (source.Spatial) ++spatialCount;
            if (source.PlayOnStart) ++startedCount;
        }

        QUIET_CORE_INFO("Created {} runtime audio sources. spatial={}, playOnStart={}", m_Sources.size(), spatialCount, startedCount);
    }

    void AudioWorld::ApplyMixSettings(const Scene::SceneAudioSettings &settings)
    {
        if (!m_AudioSubsystem.IsInitialized()) return;
        if (!m_MixSettingsApplied)
        {
            m_PreviousMasterVolume = m_AudioSubsystem.GetBusVolume(AudioBus::Master);
            m_PreviousMusicVolume = m_AudioSubsystem.GetBusVolume(AudioBus::Music);
            m_PreviousSFXVolume = m_AudioSubsystem.GetBusVolume(AudioBus::SFX);
            m_PreviousUIVolume = m_AudioSubsystem.GetBusVolume(AudioBus::UI);
            m_MixSettingsApplied = true;
        }

        const auto validatedVolume = [](const float value)
        {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 1.0f;
        };

        const bool masterApplied = m_AudioSubsystem.SetBusVolume(AudioBus::Master, validatedVolume(settings.MasterVolume));
        const bool musicApplied = m_AudioSubsystem.SetBusVolume(AudioBus::Music, validatedVolume(settings.MusicVolume));
        const bool sfxApplied = m_AudioSubsystem.SetBusVolume(AudioBus::SFX, validatedVolume(settings.SFXVolume));
        const bool uiApplied = m_AudioSubsystem.SetBusVolume(AudioBus::UI, validatedVolume(settings.UIVolume));

        if (!masterApplied || !musicApplied || !sfxApplied || !uiApplied)
        {
            QUIET_CORE_WARN("One or more runtime audio mix levels could not be applied.");
        }
    }

    void AudioWorld::Update(entt::registry& registry, const entt::entity listenerEntity, const float deltaTime)
    {
        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine) return;

        const bool validDeltaTime = std::isfinite(deltaTime) && deltaTime > 0.0f;
        UpdateListener(registry, listenerEntity, deltaTime);
        SynchronizeSources(registry);

        for (SourceBinding& binding : m_Sources)
        {
            if (!binding.Sound || !binding.Spatial) continue;
            if (!registry.valid(binding.Entity)) continue;

            const auto* transformComponent = registry.try_get<Scene::TransformComponent>(binding.Entity);
            if (!transformComponent) continue;

            const auto& transform = transformComponent->WorldTransform;
            const glm::vec3 currentPosition = transform.Position;
            ma_sound_set_position(binding.Sound.get(), currentPosition.x, currentPosition.y, currentPosition.z);

            glm::vec3 velocity{ 0.0f };

            const bool currentPositionValid =
                std::isfinite(currentPosition.x) &&
                std::isfinite(currentPosition.y) &&
                std::isfinite(currentPosition.z);

            const bool previousPositionValid =
                std::isfinite(binding.PreviousPosition.x) &&
                std::isfinite(binding.PreviousPosition.y) &&
                std::isfinite(binding.PreviousPosition.z);

            if (validDeltaTime && binding.HasPreviousPosition && currentPositionValid && previousPositionValid)
            {
                velocity = (currentPosition - binding.PreviousPosition) / deltaTime;
            }

            ma_sound_set_velocity(binding.Sound.get(), velocity.x, velocity.y, velocity.z);

            if (currentPositionValid)
            {
                binding.PreviousPosition = currentPosition;
                binding.HasPreviousPosition = true;
            }
            else
            {
                binding.HasPreviousPosition = false;
            }

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

    bool AudioWorld::CreateSourceBinding(entt::registry &registry, entt::entity entity)
    {
        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine || !registry.valid(entity))
            return false;

        if (!registry.all_of<Scene::ActorIdentityComponent, Scene::TransformComponent, Scene::AudioSourceComponent>(entity))
            return false;

        if (FindSourceBinding(entity)) return false;

        const auto& identity = registry.get<Scene::ActorIdentityComponent>(entity);
        const auto& transform = registry.get<Scene::TransformComponent>(entity).WorldTransform;
        const auto& source = registry.get<Scene::AudioSourceComponent>(entity);

        if (source.AudioAssetID.empty()) return false;
        const auto asset = m_AudioSubsystem.GetLibrary().Get(source.AudioAssetID);

        if (!asset.has_value())
        {
            QUIET_CORE_WARN("Cannot create runtime audio source for actor '{}': audio asset '{}' is not loaded.", Core::UUIDToString(identity.ID), source.AudioAssetID);
            return false;
        }

        auto sound = std::make_unique<ma_sound>();
        const std::string path = asset->SourcePath.string();

        ma_uint32 flags = MA_SOUND_FLAG_STREAM;
        if (!source.Spatial)
        {
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
        }

        ma_sound_group* soundGroup = m_AudioSubsystem.GetSoundGroup(source.Bus);

        const ma_result initializedResult = ma_sound_init_from_file(m_AudioSubsystem.m_Engine.get(), path.c_str(), flags, soundGroup, nullptr, sound.get());
        if (initializedResult != MA_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to initialize runtime audio source '{}' for actor '{}': {}", source.AudioAssetID, Core::UUIDToString(identity.ID), ma_result_description(initializedResult));
            return false;
        }

        const float volume = std::isfinite(source.Volume) ? std::clamp(source.Volume, 0.0f, 4.0f) : 1.0f;
        const float pitch = std::isfinite(source.Pitch) ? std::clamp(source.Pitch, 0.01f, 4.0f) : 1.0f;

        ma_sound_set_volume(sound.get(), volume);
        ma_sound_set_pitch(sound.get(), pitch);
        ma_sound_set_looping(sound.get(), source.Loop ? MA_TRUE : MA_FALSE);

        if (source.Spatial)
        {
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

            const float dopplerFactor = std::isfinite(source.DopplerFactor) ? std::clamp(source.DopplerFactor, 0.0f, 10.0f) : 1.0f;

            ma_sound_set_velocity(sound.get(), 0.0f, 0.0f, 0.0f);
            ma_sound_set_doppler_factor(sound.get(), dopplerFactor);
        }

        if (source.PlayOnStart)
        {
            const ma_result startResult = ma_sound_start(sound.get());
            if (startResult != MA_SUCCESS)
            {
                ma_sound_uninit(sound.get());

                QUIET_CORE_WARN("Failed to start runtime audio source '{}' for actor '{}': {}", source.AudioAssetID, Core::UUIDToString(identity.ID), ma_result_description(startResult));
                return false;
            }
        }

        SourceBinding binding{};
        binding.Entity = entity;
        binding.ActorID = identity.ID;
        binding.AudioAssetID = source.AudioAssetID;
        binding.Bus = source.Bus;
        binding.Sound = std::move(sound);
        binding.Spatial = source.Spatial;

        if (source.Spatial)
        {
            binding.PreviousPosition = transform.Position;
            binding.HasPreviousPosition = true;
        }

        m_Sources.push_back(std::move(binding));
        return true;
    }

    void AudioWorld::DestroySourceBinding(SourceBinding &binding)
    {
        if (!binding.Sound) return;

        (void)ma_sound_stop(binding.Sound.get());

        ma_sound_uninit(binding.Sound.get());
        binding.Sound.reset();
        binding.Paused = false;
        binding.HasPreviousPosition = false;
    }

    void AudioWorld::SynchronizeSources(entt::registry &registry)
    {
        std::size_t removedCount = 0;
        std::size_t createdCount = 0;

        // Remove bindings which no longer correspond to the current runtime ECS state.
        for (std::size_t index = 0; index < m_Sources.size();)
        {
            SourceBinding& binding = m_Sources[index];
            bool removeBinding = !registry.valid(binding.Entity);

            if (!removeBinding)
            {
                const auto* identity = registry.try_get<Scene::ActorIdentityComponent>(binding.Entity);
                removeBinding = identity == nullptr || identity->ID != binding.ActorID;
            }

            if (!removeBinding)
            {
                const auto* source = registry.try_get<Scene::AudioSourceComponent>(binding.Entity);
                removeBinding =
                    source == nullptr ||
                    source->AudioAssetID.empty() ||
                    source->AudioAssetID != binding.AudioAssetID ||
                    source->Bus != binding.Bus;
            }
            if (!removeBinding)
            {
                ++index;
                continue;
            }

            DestroySourceBinding(binding);
            m_Sources.erase(m_Sources.begin() + static_cast<std::ptrdiff_t>(index));
            ++removedCount;
        }

        // New runtime AudioSourceComponents.
        auto view = registry.view<Scene::ActorIdentityComponent, Scene::TransformComponent, Scene::AudioSourceComponent>();
        for (const entt::entity entity : view)
        {
            if (FindSourceBinding(entity)) continue;

            const auto& source = registry.get<Scene::AudioSourceComponent>(entity);
            if (source.AudioAssetID.empty()) continue;

            // Skip unresolved IDs without attempting backend initialization every frame.
            if (!m_AudioSubsystem.GetLibrary().Has(source.AudioAssetID)) continue;
            if (CreateSourceBinding(registry, entity))
            {
                ++createdCount;
            }
        }

        if (createdCount > 0 || removedCount > 0)
        {
            QUIET_CORE_INFO("Synchronized runtime audio sources. created={}, removed={}, active={}", createdCount, removedCount, m_Sources.size());
        }
    }

    void AudioWorld::UpdateListener(entt::registry& registry, const entt::entity listenerEntity, const float deltaTime)
    {
        if (!m_AudioSubsystem.IsInitialized() || !m_AudioSubsystem.m_Engine) return;

        constexpr ma_uint32 listenerIndex = 0;
        if (listenerEntity == entt::null || !registry.valid(listenerEntity))
        {
            ma_engine_listener_set_velocity(m_AudioSubsystem.m_Engine.get(), listenerIndex, 0.0f, 0.0f, 0.0f);
            ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_FALSE);

            m_ListenerEntity = entt::null;
            m_HasPreviousListenerPosition = false;
            return;
        }

        const auto* transformComponent = registry.try_get<Scene::TransformComponent>(listenerEntity);
        if (!transformComponent)
        {
            ma_engine_listener_set_velocity(m_AudioSubsystem.m_Engine.get(), listenerIndex, 0.0f, 0.0f, 0.0f);
            ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_FALSE);

            m_ListenerEntity = entt::null;
            m_HasPreviousListenerPosition = false;
            return;
        }

        const auto& transform = transformComponent->WorldTransform;
        const glm::vec3 currentPosition = transform.Position;
        glm::vec3 velocity{ 0.0f };

        const bool validDeltaTime = std::isfinite(deltaTime) && deltaTime > 0.0f;

        const bool currentPositionValid =
            std::isfinite(currentPosition.x) &&
            std::isfinite(currentPosition.y) &&
            std::isfinite(currentPosition.z);

        const bool previousPositionValid =
            std::isfinite(m_PreviousListenerPosition.x) &&
            std::isfinite(m_PreviousListenerPosition.y) &&
            std::isfinite(m_PreviousListenerPosition.z);

        const bool sameListener = m_ListenerEntity == listenerEntity;
        if (sameListener && validDeltaTime && m_HasPreviousListenerPosition && currentPositionValid && previousPositionValid)
        {
            velocity = (currentPosition - m_PreviousListenerPosition) / deltaTime;
        }

        ma_engine_listener_set_enabled(m_AudioSubsystem.m_Engine.get(), listenerIndex, MA_TRUE);
        ma_engine_listener_set_position(m_AudioSubsystem.m_Engine.get(), listenerIndex, currentPosition.x, currentPosition.y, currentPosition.z);
        ma_engine_listener_set_velocity(m_AudioSubsystem.m_Engine.get(), listenerIndex, velocity.x, velocity.y, velocity.z);

        glm::mat4 rotation(1.0f);
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));

        const glm::vec3 forward = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        const glm::vec3 up = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));

        ma_engine_listener_set_direction(m_AudioSubsystem.m_Engine.get(), listenerIndex, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(m_AudioSubsystem.m_Engine.get(), listenerIndex, up.x, up.y, up.z);

        m_ListenerEntity = listenerEntity;

        if (currentPositionValid)
        {
            m_PreviousListenerPosition = currentPosition;
            m_HasPreviousListenerPosition = true;
        }
        else
        {
            m_HasPreviousListenerPosition = false;
        }
    }

    void AudioWorld::RestoreMixSettings()
    {
        if (!m_MixSettingsApplied) return;
        if (m_AudioSubsystem.IsInitialized())
        {
            (void)m_AudioSubsystem.SetBusVolume(AudioBus::Master, m_PreviousMasterVolume);
            (void)m_AudioSubsystem.SetBusVolume(AudioBus::Music, m_PreviousMusicVolume);
            (void)m_AudioSubsystem.SetBusVolume(AudioBus::SFX, m_PreviousSFXVolume);
            (void)m_AudioSubsystem.SetBusVolume(AudioBus::UI, m_PreviousUIVolume);
        }

        m_MixSettingsApplied = false;
    }

    void AudioWorld::DestroySources()
    {
        for (SourceBinding& binding : m_Sources)
        {
            DestroySourceBinding(binding);
        }
        m_Sources.clear();
    }
}
