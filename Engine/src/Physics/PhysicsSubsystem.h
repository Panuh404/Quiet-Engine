#pragma once

#include <memory>

namespace JPH
{
    class TempAllocatorImpl;
    class JobSystemThreadPool;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem
    {
    public:
        PhysicsSubsystem();
        ~PhysicsSubsystem();

        PhysicsSubsystem(const PhysicsSubsystem&) = delete;
        PhysicsSubsystem& operator=(const PhysicsSubsystem&) = delete;
        PhysicsSubsystem(PhysicsSubsystem&&) = delete;
        PhysicsSubsystem& operator=(PhysicsSubsystem&&) = delete;

        [[nodiscard]] bool IsInitialized() const { return m_Initialized;}

    private:
        void Initialize();
        void Shutdown();

    private:
        std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

        bool m_Initialized = false;
    };
}