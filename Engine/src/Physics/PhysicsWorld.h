#pragma once

#include <memory>
#include <vector>
#include <glm/vec3.hpp>

#include <entt/entity/registry.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

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

        void BuildBodies(entt::registry& registry);
        void Update(float deltaTime, entt::registry& registry);

        bool ApplyImpulse(entt::entity entity, const glm::vec3& impulse);
        bool SetLinearVelocity(entt::entity entity, const glm::vec3& velocity);

    private:
        struct BodyBinding
        {
            entt::entity Entity = entt::null;
            JPH::BodyID BodyID{};
        };
        [[nodiscard]] const BodyBinding* FindBodyBinding(entt::entity entity) const;

        void DestroyBodies();
        void SyncKinematicBodies(entt::registry& registry, float timeStep);
        void SyncDynamicBodies(entt::registry& registry);

    private:
        PhysicsSubsystem& m_Subsystem;

        std::unique_ptr<JPH::BroadPhaseLayerInterface> m_BroadPhaseLayerInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_ObjectVsBroadPhaseLayerFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> m_ObjectLayerPairFilter;
        std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;

        std::vector<BodyBinding> m_Bodies;

        float m_Accumulator = 0.0f;
    };
}