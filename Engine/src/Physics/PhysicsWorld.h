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

    /**
     * @brief Owns one Scene's runtime physics simulation and body bindings.
     * @ingroup quiet_physics
     *
     * Borrows an initialized PhysicsSubsystem and receives registry references
     * for body synchronization and simulation updates.
     */
    class PhysicsWorld
    {
    public:
        explicit PhysicsWorld(PhysicsSubsystem & subsystem, const glm::vec3& gravity);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        /**
         * @brief Rebuilds runtime bodies from eligible registry entities.
         * @note Requires identity, transform, rigid body, and exactly one collider.
         * @note Invalid candidates are skipped; no aggregate success is returned.
         */
        void BuildBodies(entt::registry& registry);

        /**
         * @brief Removes invalid bindings and creates missing eligible bodies.
         * @note Does not refresh existing shapes, motion types, or creation settings.
         * @note Checks recorded Actor UUIDs as well as entity validity.
         */
        void SynchronizeBodies(entt::registry& registry);

        /**
         * @brief Synchronizes bodies and advances fixed-step physics.
         * @param deltaTime Elapsed seconds accumulated up to four 60 Hz steps.
         * @param registry Scene registry supplying bodies and receiving transforms.
         * @note Body synchronization precedes delta validation.
         * @note Excess elapsed time is discarded; no transform interpolation is added.
         * @note Jolt update error masks are logged without stopping subsequent steps.
         */
        void Update(float deltaTime, entt::registry& registry);

        /**
         * @brief Applies an impulse to a bound Dynamic physics body.
         * @return True when the command is issued; false for unavailable bodies,
         *         non-Dynamic bodies, or non-finite input.
         * @note Checks the actual Jolt motion type.
         */
        bool ApplyImpulse(entt::entity entity, const glm::vec3& impulse);

        /**
         * @brief Sets the linear velocity of a bound Dynamic physics body.
         * @return True when the command is issued; false for unavailable bodies,
         *         non-Dynamic bodies, or non-finite input.
         * @note Actor transforms are updated through normal physics synchronization.
         */
        bool SetLinearVelocity(entt::entity entity, const glm::vec3& velocity);

        /**
         * @brief Drains the buffered contact events.
         * @return Events queued since the previous drain or queue clear.
         * @note Events retain callback arrival order.
         */
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