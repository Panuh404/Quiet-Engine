#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <glm/vec3.hpp>

#include <entt/entity/registry.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Physics/PhysicsContactEvent.h"

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
        explicit PhysicsWorld(PhysicsSubsystem & subsystem, const glm::vec3& gravity);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        void BuildBodies(entt::registry& registry);
        void SynchronizeBodies(entt::registry& registry);
        void Update(float deltaTime, entt::registry& registry);

        bool ApplyImpulse(entt::entity entity, const glm::vec3& impulse);
        bool SetLinearVelocity(entt::entity entity, const glm::vec3& velocity);

        [[nodiscard]] std::vector<PhysicsContactEvent> ConsumeContactEvents();

    private:
        class ContactListenerImpl;
        struct BodyBinding
        {
            entt::entity Entity = entt::null;
            JPH::BodyID BodyID{};
            Core::UUID ActorID{};
        };

        [[nodiscard]] const BodyBinding* FindBodyBinding(entt::entity entity) const;
        [[nodiscard]] const BodyBinding* FindBodyBinding(JPH::BodyID bodyID) const;
        [[nodiscard]] bool WereBodiesInContact(JPH::BodyID bodyA, JPH::BodyID bodyB) const;

        void QueueContactEvent(JPH::BodyID bodyA, JPH::BodyID bodyB, PhysicsContactEventType type);
        bool CreateBodyBinding(entt::registry& registry, entt::entity entity);
        void DestroyBodyBinding(BodyBinding& binding);

        void DestroyBodies();

        void SyncKinematicBodies(entt::registry& registry, float timeStep);
        void SyncDynamicBodies(entt::registry& registry);

    private:
        PhysicsSubsystem& m_Subsystem;

        std::unique_ptr<JPH::BroadPhaseLayerInterface> m_BroadPhaseLayerInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_ObjectVsBroadPhaseLayerFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> m_ObjectLayerPairFilter;
        std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;

        std::unique_ptr<ContactListenerImpl> m_ContactListener;
        std::mutex m_ContactEventsMutex;
        std::vector<PhysicsContactEvent> m_ContactEvents;

        std::vector<BodyBinding> m_Bodies;

        float m_Accumulator = 0.0f;
    };
}