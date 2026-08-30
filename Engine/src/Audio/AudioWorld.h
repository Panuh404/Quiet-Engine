#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <entt/entt.hpp>

#include "Audio/AudioBus.h"
#include "Quiet/Core/UUID.h"

struct ma_sound;

namespace Quiet::Scene
{
    struct SceneAudioSettings;
}

namespace Quiet::Audio
{
    class AudioSubsystem;

    enum class AudioPlaybackState
    {
        Unavailable,
        Stopped,
        Playing,
        Paused
    };

    class AudioWorld
    {
    public:
        explicit AudioWorld(AudioSubsystem& audioSubsystem);
        ~AudioWorld();

        AudioWorld(const AudioWorld&) = delete;
        AudioWorld& operator=(const AudioWorld&) = delete;
        AudioWorld(AudioWorld&&) = delete;
        AudioWorld& operator=(AudioWorld&&) = delete;

        void BuildSources(entt::registry& registry, entt::entity listenerEntity);
        void ApplyMixSettings(const Scene::SceneAudioSettings& settings);
        void Update(entt::registry& registry, entt::entity listenerEntity, float deltaTime);

        bool PlaySource(entt::entity entity);
        bool StopSource(entt::entity entity);
        bool PauseSource(entt::entity entity);
        bool ResumeSource(entt::entity entity);
        bool SeekSource(entt::entity entity, float seconds);

        [[nodiscard]] AudioPlaybackState GetSourcePlaybackState(entt::entity entity) const;
        [[nodiscard]] std::size_t GetSourceCount() const { return m_Sources.size(); }

    private:
        struct SourceBinding
        {
            entt::entity Entity = entt::null;
            AudioBus Bus = AudioBus::SFX;
            Core::UUID ActorID{};

            std::string AudioAssetID;
            std::unique_ptr<ma_sound> Sound;

            glm::vec3 PreviousPosition{ 0.0f };

            bool Spatial = false;
            bool HasPreviousPosition = false;
            bool Paused = false;
        };

        [[nodiscard]] SourceBinding* FindSourceBinding(entt::entity entity);
        [[nodiscard]] const SourceBinding* FindSourceBinding(entt::entity entity) const;

        bool CreateSourceBinding(entt::registry& registry, entt::entity entity);
        void DestroySourceBinding(SourceBinding& binding);
        void SynchronizeSources(entt::registry& registry);

        void UpdateListener(entt::registry& registry, entt::entity listenerEntity, float deltaTime);
        void RestoreMixSettings();
        void DestroySources();

    private:
        AudioSubsystem& m_AudioSubsystem;
        std::vector<SourceBinding> m_Sources;

        entt::entity m_ListenerEntity = entt::null;
        glm::vec3 m_PreviousListenerPosition{ 0.0f };
        bool m_HasPreviousListenerPosition = false;

        float m_PreviousMasterVolume = 1.0f;
        float m_PreviousMusicVolume = 1.0f;
        float m_PreviousSFXVolume = 1.0f;
        float m_PreviousUIVolume = 1.0f;
        bool m_MixSettingsApplied = false;
    };
}