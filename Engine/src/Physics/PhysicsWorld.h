#pragma once

#include <memory>

namespace JPH
{
    class PhysicsSystem;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem;

    class PhysicsWorld
    {
    public:
        explicit PhysicsWorld(PhysicsSubsystem & subsystem);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        void Update(float deltaTime);

    private:
        PhysicsSubsystem& m_Subsystem;

        std::unique_ptr<JPH::BroadPhaseLayerInterface> m_BroadPhaseLayerInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_ObjectVsBroadPhaseLayerFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> m_ObjectLayerPairFilter;
        std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;

        float m_Accumulator = 0.0f;
    };
}