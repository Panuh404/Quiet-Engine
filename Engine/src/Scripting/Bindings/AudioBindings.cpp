#include "Scripting/Bindings/Bindings.h"

#include <cmath>
#include <stdexcept>

#include "Audio/AudioWorld.h"
#include "Scene/Scene.h"

namespace Quiet::Scripting::Bindings
{

    void InstallAudioBindings(sol::state& lua, sol::table& actorTable, Scene::Scene& scene, const Core::UUID actorID)
    {
        sol::table audioTable = lua.create_table();

        audioTable.set_function("Play", [&scene, actorID]()
        {
            return scene.PlayAudioSource(actorID);
        });

        audioTable.set_function("Stop", [&scene, actorID]()
        {
            return scene.StopAudioSource(actorID);
        });

        audioTable.set_function("Pause", [&scene, actorID]()
        {
            return scene.PauseAudioSource(actorID);
        });

        audioTable.set_function("Resume", [&scene, actorID]()
        {
            return scene.ResumeAudioSource(actorID);
        });

        audioTable.set_function("Seek", [&scene, actorID](const float seconds)
        {
            if (!std::isfinite(seconds) || seconds < 0.0f) return false;
            return scene.SeekAudioSource(actorID, seconds);
        });

        audioTable.set_function("GetState", [&scene, actorID]()
        {
            switch (scene.GetAudioSourcePlaybackState(actorID))
                {
                    case Audio::AudioPlaybackState::Stopped:        return "Stopped";
                    case Audio::AudioPlaybackState::Playing:        return "Playing";
                    case Audio::AudioPlaybackState::Paused:         return "Paused";
                    case Audio::AudioPlaybackState::Unavailable:
                    default:                                        return "Unavailable";
                }
            });

        actorTable["Audio"] = audioTable;
    }
}