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