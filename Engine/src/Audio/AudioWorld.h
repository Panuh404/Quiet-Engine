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

    /**
     * @brief Describes runtime source availability and playback state.
     * @ingroup quiet_audio
     */
    enum class AudioPlaybackState
    {
        Unavailable,
        Stopped,
        Playing,
        Paused
    };

    /**
     * @brief Owns one Scene's runtime audio sounds and source bindings.
     * @ingroup quiet_audio
     *
     * Borrows AudioSubsystem and receives registry references for source and
     * listener updates. Uses the subsystem's shared playback engine and buses.
     */
    class AudioWorld
    {
    public:
        explicit AudioWorld(AudioSubsystem& audioSubsystem);
        ~AudioWorld();

        AudioWorld(const AudioWorld&) = delete;
        AudioWorld& operator=(const AudioWorld&) = delete;
        AudioWorld(AudioWorld&&) = delete;
        AudioWorld& operator=(AudioWorld&&) = delete;

        /**
         * @brief Rebuilds runtime sounds from eligible Scene entities.
         * @note Destroys existing sources before checking subsystem availability.
         * @note Establishes the listener before creating PlayOnStart sounds.
         * @note Failed candidates remain unbound; no aggregate success is returned.
         */
        void BuildSources(entt::registry& registry, entt::entity listenerEntity);

        /**
         * @brief Applies Scene levels to shared subsystem buses.
         * @note Saves previous levels on first application for teardown restoration.
         * @note Finite levels are clamped to 0–4; nonfinite levels use 1.
         * @note Failed bus updates do not roll back successful changes.
         */
        void ApplyMixSettings(const Scene::SceneAudioSettings& settings);

        /**
         * @brief Updates the listener, source membership, and spatial transforms.
         * @param registry Scene registry supplying source and transform data.
         * @param listenerEntity Selected listener entity, or entt::null.
         * @param deltaTime Elapsed seconds used for velocity estimation.
         * @note Identity, asset ID, and bus changes can recreate source bindings.
         * @note Other authored sound settings are not refreshed on existing bindings.
         * @note Invalid delta time does not skip membership synchronization.
         */
        void Update(entt::registry& registry, entt::entity listenerEntity, float deltaTime);

        /**
         * @brief Stops, rewinds, and starts an existing source.
         * @return True when all requested backend operations succeed.
         * @note Failure can leave the source stopped or rewound.
         */
        bool PlaySource(entt::entity entity);

        /**
         * @brief Stops an existing source and rewinds it to frame zero.
         * @return True when stopping and rewinding succeed.
         * @note A rewind failure can leave the source stopped.
         */
        bool StopSource(entt::entity entity);

        /**
         * @brief Pauses a currently playing source without rewinding.
         * @return False when unavailable, not playing, or stopping fails.
         */
        bool PauseSource(entt::entity entity);

        /**
         * @brief Resumes a source previously marked paused.
         * @return False when unavailable, not paused, or starting fails.
         */
        bool ResumeSource(entt::entity entity);

        /**
         * @brief Seeks an existing source to a time in seconds.
         * @param seconds Finite, nonnegative target time.
         * @return True when the backend accepts the seek.
         * @note Does not explicitly change playback or paused state.
         * @note Does not clamp against library duration metadata.
         */
        bool SeekSource(entt::entity entity, float seconds);

        /**
         * @brief Reports availability, backend playback, and tracked pause state.
         * @note Stopped includes both explicit stopping and natural completion.
         */
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