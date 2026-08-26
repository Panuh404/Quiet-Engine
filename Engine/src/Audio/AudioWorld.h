#pragma once

#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "Quiet/Core/UUID.h"

struct ma_sound;

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

        void BuildSources(entt::registry& registry);

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
            Core::UUID ActorID{};

            std::string AudioAssetID;
            std::unique_ptr<ma_sound> Sound;

            bool Paused = false;
        };

        [[nodiscard]] SourceBinding* FindSourceBinding(entt::entity entity);
        [[nodiscard]] const SourceBinding* FindSourceBinding(entt::entity entity) const;

        void DestroySources();

    private:
        AudioSubsystem& m_AudioSubsystem;
        std::vector<SourceBinding> m_Sources;
    };
}