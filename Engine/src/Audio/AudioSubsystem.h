#pragma once

#include <memory>

struct ma_engine;

namespace Quiet::Audio
{
    class AudioSubsystem
    {
    public:
        AudioSubsystem();
        ~AudioSubsystem();

        AudioSubsystem(const AudioSubsystem&) = delete;
        AudioSubsystem& operator=(const AudioSubsystem&) = delete;
        AudioSubsystem(AudioSubsystem&&) = delete;
        AudioSubsystem& operator=(AudioSubsystem&&) = delete;

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        void Initialize();
        void Shutdown();

    private:
        std::unique_ptr<ma_engine> m_Engine;
        bool m_Initialized = false;
    };
}