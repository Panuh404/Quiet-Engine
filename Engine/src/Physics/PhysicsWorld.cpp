#include "Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/trigonometric.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "Scene/Components/ActorIdentityComponent.h"
#include "Scene/Components/BoxColliderComponent.h"
#include "Scene/Components/RigidBodyComponent.h"
#include "Scene/Components/TransformComponent.h"

#include "Physics/PhysicsSubsystem.h"
#include "Quiet/Core/Log.h"

namespace
{
#pragma region Helpers

    bool IsFiniteVec3(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    JPH::Vec3 ToJoltVec3(const glm::vec3& value)
    {
        return { value.x, value.y, value.z };
    }

    JPH::RVec3 ToJoltPosition(const glm::vec3& value)
    {
        return { value.x, value.y, value.z };
    }

    JPH::Quat ToJoltRotation(const glm::vec3& rotationDegrees)
    {
        const glm::vec3 radians = glm::radians(rotationDegrees);
        return JPH::Quat::sEulerAngles(JPH::Vec3(radians.x, radians.y, radians.z));
    }

    glm::vec3 ToQuietPosition(const JPH::RVec3& position)
    {
        return { position.GetX(), position.GetY(), position.GetZ() };
    }

    glm::vec3 ToQuietRotationDegrees(const JPH::Quat& rotation)
    {
        const JPH::Vec3 radians = rotation.GetEulerAngles();
        return glm::degrees(glm::vec3(radians.GetX(), radians.GetY(), radians.GetZ()));
    }

    JPH::EMotionType ToJoltMotionType(const Quiet::Scene::RigidBodyMotionType type)
    {
        using Quiet::Scene::RigidBodyMotionType;
        switch (type)
        {
            case RigidBodyMotionType::Static:       return JPH::EMotionType::Static;
            case RigidBodyMotionType::Kinematic:    return JPH::EMotionType::Kinematic;
            case RigidBodyMotionType::Dynamic:
            default:                                return JPH::EMotionType::Dynamic;
        }
    }

#pragma endregion

    namespace ObjectLayers
    {
        static constexpr JPH::ObjectLayer NonMoving = 0;
        static constexpr JPH::ObjectLayer Moving = 1;
        static constexpr JPH::ObjectLayer Count = 2;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NonMoving{ 0 };
        static constexpr JPH::BroadPhaseLayer Moving{ 1 };
        static constexpr JPH::uint            Count = 2;
    }

    class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BroadPhaseLayerInterfaceImpl()
        {
            m_ObjectToBroadPhase[ObjectLayers::NonMoving] = BroadPhaseLayers::NonMoving;
            m_ObjectToBroadPhase[ObjectLayers::Moving] = BroadPhaseLayers::Moving;
        }

        [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::Count;
        }

        [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            JPH_ASSERT(layer < ObjectLayers::Count);
            return m_ObjectToBroadPhase[layer];
        }

        #if defined(JPH_EXTERNAL_PROFILE) || defined (JPH_PROFILE_ENABLED)
        [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
        {
            if (layer == BroadPhaseLayers::NonMoving) return "NON_MOVING";
            if (layer == BroadPhaseLayers::Moving) return "MOVING";
            JPH_ASSERT(false);
            return "INVALID";
        }
        #endif

    private:
        JPH::BroadPhaseLayer m_ObjectToBroadPhase[ObjectLayers::Count];
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
        {
            switch (first)
            {
                case ObjectLayers::NonMoving:
                    return second == ObjectLayers::Moving;

                case ObjectLayers::Moving:
                    return true;

                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer objectLayer, JPH::BroadPhaseLayer broadPhaseLayer) const override
        {
            switch (objectLayer)
            {
                case ObjectLayers::NonMoving:
                    return broadPhaseLayer == BroadPhaseLayers::Moving;

                case ObjectLayers::Moving:
                    return true;

                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };
}

namespace Quiet::Physics
{
    PhysicsWorld::PhysicsWorld(PhysicsSubsystem &subsystem) : m_Subsystem(subsystem)
    {
        if (!m_Subsystem.IsInitialized())
            throw std::runtime_error("Cannot create PhysicsWorld before PhysicsSubsystem initialization.");

        m_BroadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
        m_ObjectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
        m_ObjectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
        m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();

        constexpr JPH::uint maxBodies = 65536;
        constexpr JPH::uint numBodyMutexes = 0;
        constexpr JPH::uint maxBodyPairs = 65536;
        constexpr JPH::uint maxContactConstraints = 10240;

        m_PhysicsSystem->Init(
            maxBodies,
            numBodyMutexes,
            maxBodyPairs,
            maxContactConstraints,
            *m_BroadPhaseLayerInterface,
            *m_ObjectVsBroadPhaseLayerFilter,
            *m_ObjectLayerPairFilter
        );

        m_PhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

        QUIET_CORE_INFO("Created physics world. gravity=(0.00, -9.81, 0.00), fixedStep=60Hz, maxBodies={}", maxBodies);
    }

    PhysicsWorld::~PhysicsWorld()
    {
        DestroyBodies();

        m_PhysicsSystem.reset();
        m_ObjectLayerPairFilter.reset();
        m_ObjectVsBroadPhaseLayerFilter.reset();
        m_BroadPhaseLayerInterface.reset();

        QUIET_CORE_INFO("Destroyed physics world.");
    }

    void PhysicsWorld::BuildBodies(entt::registry &registry)
    {
        DestroyBodies();

        if (!m_PhysicsSystem) return;
        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

        auto view = registry.view<Scene::ActorIdentityComponent, Scene::TransformComponent, Scene::RigidBodyComponent>();
        for (const entt::entity entity : view)
        {
            const auto& identity = registry.get<Scene::ActorIdentityComponent>(entity);
            const auto& transform = registry.get<Scene::TransformComponent>(entity).WorldTransform;
            const auto& rigidBody = registry.get<Scene::RigidBodyComponent>(entity);
            const auto* collider = registry.try_get<Scene::BoxColliderComponent>(entity);

            if (!collider)
            {
                QUIET_CORE_WARN("Skipping physics body for '{}': no supported collider.", identity.Name);
                continue;
            }

            constexpr float minimumSize = 0.001f;
            glm::vec3 effectiveSize = glm::abs(collider->Size * transform.Scale);
            effectiveSize = glm::max(effectiveSize, glm::vec3(minimumSize));

            const glm::vec3 halfExtent = effectiveSize * 0.5f;
            JPH::ShapeRefC shape = new JPH::BoxShape(ToJoltVec3(halfExtent));

            const glm::vec3 effectiveCenter = collider->Center * transform.Scale;
            if (glm::dot(effectiveCenter, effectiveCenter) > 1.0e-12f)
            {
                shape = new JPH::RotatedTranslatedShape(ToJoltVec3(effectiveCenter), JPH::Quat::sIdentity(), shape);
            }

            const JPH::EMotionType motionType = ToJoltMotionType(rigidBody.MotionType);
            const JPH::ObjectLayer objectLayer = motionType == JPH::EMotionType::Static ? ObjectLayers::NonMoving : ObjectLayers::Moving;

            JPH::BodyCreationSettings settings(shape, ToJoltPosition(transform.Position), ToJoltRotation(transform.RotationDegrees), motionType, objectLayer);
            settings.mFriction = std::isfinite(rigidBody.Friction) ? std::max(rigidBody.Friction, 0.0f) : 0.2f;
            settings.mRestitution = std::isfinite(rigidBody.Restitution) ? std::clamp(rigidBody.Restitution, 0.0f, 1.0f) : 0.0f;
            settings.mLinearDamping = std::isfinite(rigidBody.LinearDamping) ? std::max(rigidBody.LinearDamping, 0.0f) : 0.05f;
            settings.mAngularDamping = std::isfinite(rigidBody.AngularDamping) ? std::max(rigidBody.AngularDamping, 0.0f) : 0.05f;
            settings.mGravityFactor = std::isfinite(rigidBody.GravityFactor) ? rigidBody.GravityFactor : 1.0f;
            settings.mAllowSleeping = rigidBody.AllowSleeping;

            const JPH::EActivation activation = motionType == JPH::EMotionType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
            const JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, activation);

            if (bodyID.IsInvalid())
            {
                QUIET_CORE_ERROR("Failed to create Jolt body for actor '{}'.", identity.Name);
                continue;
            }

            m_Bodies.push_back(BodyBinding{ entity, bodyID });
        }

        if (!m_Bodies.empty())
        {
            m_PhysicsSystem->OptimizeBroadPhase();
        }

        QUIET_CORE_INFO("Created {} runtime physics bodies.", m_Bodies.size());
    }

    void PhysicsWorld::Update(float deltaTime, entt::registry& registry)
    {
        if (!m_PhysicsSystem || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

        constexpr float fixedTimeStep = 1.0f / 60.0f;
        constexpr int maxStepsPerFrame = 4;
        constexpr float maxAccumulatedTime = fixedTimeStep * static_cast<float>(maxStepsPerFrame);

        m_Accumulator = std::min(m_Accumulator + deltaTime, maxAccumulatedTime);

        bool stepped = false;
        while (m_Accumulator >= fixedTimeStep)
        {
            SyncKinematicBodies(registry, fixedTimeStep);
            const JPH::EPhysicsUpdateError updateError = m_PhysicsSystem->Update(fixedTimeStep, 1, &m_Subsystem.GetTempAllocator(), &m_Subsystem.GetJobSystem());

            if (updateError != JPH::EPhysicsUpdateError::None)
            {
                QUIET_CORE_WARN("Jolt physics update reported error mask={}.", static_cast<std::uint32_t>(updateError));
            }

            m_Accumulator -= fixedTimeStep;
            stepped = true;
        }

        if (stepped)
        {
            SyncDynamicBodies(registry);
        }
    }

    bool PhysicsWorld::ApplyImpulse(entt::entity entity, const glm::vec3 &impulse)
    {
        if (!m_PhysicsSystem || !IsFiniteVec3(impulse)) return false;

        const BodyBinding* binding = FindBodyBinding(entity);
        if (!binding || binding->BodyID.IsInvalid()) return false;

        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        if (bodyInterface.GetMotionType(binding->BodyID) != JPH::EMotionType::Dynamic) return false;

        bodyInterface.AddImpulse(binding->BodyID, ToJoltVec3(impulse));
        return true;
    }

    bool PhysicsWorld::SetLinearVelocity(entt::entity entity, const glm::vec3 &velocity)
    {
        if (!m_PhysicsSystem || !IsFiniteVec3(velocity)) return false;

        const BodyBinding* binding = FindBodyBinding(entity);
        if (!binding || binding->BodyID.IsInvalid()) return false;

        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        if (bodyInterface.GetMotionType(binding->BodyID) != JPH::EMotionType::Dynamic) return false;

        bodyInterface.SetLinearVelocity(binding->BodyID, ToJoltVec3(velocity));
        return true;
    }

    const PhysicsWorld::BodyBinding* PhysicsWorld::FindBodyBinding(const entt::entity entity) const
    {
        for (const BodyBinding& binding : m_Bodies)
        {
            if (binding.Entity == entity)
            {
                return &binding;
            }
        }
        return nullptr;
    }

    void PhysicsWorld::DestroyBodies()
    {
        if (!m_PhysicsSystem)
        {
            m_Bodies.clear();
            return;
        }

        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        for (const BodyBinding& binding : m_Bodies)
        {
            if (binding.BodyID.IsInvalid()) continue;
            if (bodyInterface.IsAdded(binding.BodyID))
            {
                bodyInterface.RemoveBody(binding.BodyID);
            }

            bodyInterface.DestroyBody(binding.BodyID);
        }

        m_Bodies.clear();
    }

    void PhysicsWorld::SyncKinematicBodies(entt::registry &registry, float timeStep)
    {
        if (!m_PhysicsSystem || !std::isfinite(timeStep) || timeStep <= 0.0f) return;

        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        for (const BodyBinding& binding : m_Bodies)
        {
            if (binding.BodyID.IsInvalid() || !registry.valid(binding.Entity)) continue;

            const auto* rigidBody = registry.try_get<Scene::RigidBodyComponent>(binding.Entity);
            const auto* transformComponent = registry.try_get<Scene::TransformComponent>(binding.Entity);

            if (!rigidBody || !transformComponent || rigidBody->MotionType != Scene::RigidBodyMotionType::Kinematic) continue;
            const auto& transform = transformComponent->WorldTransform;

            bodyInterface.MoveKinematic(binding.BodyID, ToJoltPosition(transform.Position), ToJoltRotation(transform.RotationDegrees), timeStep);
        }
    }

    void PhysicsWorld::SyncDynamicBodies(entt::registry& registry)
    {
        if (!m_PhysicsSystem) return;

        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        for (const BodyBinding& binding : m_Bodies)
        {
            if (!registry.valid(binding.Entity)) continue;

            const auto* rigidBody = registry.try_get<Scene::RigidBodyComponent>(binding.Entity);
            auto* transformComponent = registry.try_get<Scene::TransformComponent>(binding.Entity);

            if (!rigidBody || !transformComponent || rigidBody->MotionType != Scene::RigidBodyMotionType::Dynamic) continue;

            JPH::RVec3 position{};
            JPH::Quat rotation{};

            bodyInterface.GetPositionAndRotation(binding.BodyID, position, rotation);
            auto& transform = transformComponent->WorldTransform;
            transform.Position = ToQuietPosition(position);
            transform.RotationDegrees = ToQuietRotationDegrees(rotation);
        }
    }
}
