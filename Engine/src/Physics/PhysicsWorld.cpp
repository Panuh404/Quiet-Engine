#include "Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include "Physics/PhysicsSubsystem.h"
#include "Quiet/Core/Log.h"

namespace
{
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
        // PhysicsSystem stores references to all three
        // filtering interfaces. Destroy it first.
        m_PhysicsSystem.reset();
        m_ObjectLayerPairFilter.reset();
        m_ObjectVsBroadPhaseLayerFilter.reset();
        m_BroadPhaseLayerInterface.reset();

        QUIET_CORE_INFO("Destroyed physics world.");
    }

    void PhysicsWorld::Update(float deltaTime)
    {
        if (!m_PhysicsSystem || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

        constexpr float fixedTimeStep = 1.0f / 60.0f;
        constexpr int maxStepsPerFrame = 4;
        constexpr float maxAccumulatedTime = fixedTimeStep * static_cast<float>(maxStepsPerFrame);

        m_Accumulator = std::min(m_Accumulator + deltaTime, maxAccumulatedTime);
        while (m_Accumulator >= fixedTimeStep)
        {
            const JPH::EPhysicsUpdateError updateError = m_PhysicsSystem->Update(fixedTimeStep, 1, &m_Subsystem.GetTempAllocator(), &m_Subsystem.GetJobSystem());
            if (updateError != JPH::EPhysicsUpdateError::None)
            {
                QUIET_CORE_WARN("Jolt physics update reported error mask={}.", static_cast<std::uint32_t>(updateError));
            }
            m_Accumulator -= fixedTimeStep;
        }
    }
}
