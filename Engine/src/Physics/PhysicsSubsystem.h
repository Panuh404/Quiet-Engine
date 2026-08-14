#pragma once

#include <memory>

namespace JPH
{
    class TempAllocatorImpl;
    class JobSystem;
    class JobSystemThreadPool;
}

namespace Quiet::Physics
{
    class PhysicsWorld;

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

        [[nodiscard]] JPH::TempAllocatorImpl& GetTempAllocator();
        [[nodiscard]] JPH::JobSystem& GetJobSystem();

    private:
        friend class PhysicsWorld;

        std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

        bool m_Initialized = false;
    };
}