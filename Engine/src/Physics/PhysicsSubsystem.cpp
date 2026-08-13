#include "Physics/PhysicsSubsystem.h"

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

#include "../../../Build/Intermediate/Release-Editor/_deps/jolt_physics-src/Jolt/Physics/PhysicsSettings.h"
#include "Quiet/Core/Log.h"

namespace
{
    void JoltTraceImpl(const char* format, ...)
    {
        char buffer[2048]{};

        va_list args;
        va_start(args, format);

        std::vsnprintf(buffer, sizeof(buffer), format, args);

        va_end(args);

        QUIET_CORE_TRACE("[Jolt] {}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    bool JoltAssertFailedImpl(const char* expression, const char* message, const char* file, JPH::uint line)
    {
        QUIET_CORE_ERROR(
            "[Jolt] Assertion failed at {}:{}: ({}) {}",
            file ? file : "<unknown>",
            line,
            expression ? expression : "<unknown>",
            message ? message : ""
        );
        return true;
    }
#endif
}

namespace Quiet::Physics
{
    PhysicsSubsystem::PhysicsSubsystem()
    {
        Initialize();
    }
    PhysicsSubsystem::~PhysicsSubsystem()
    {
        Shutdown();
    }

    void PhysicsSubsystem::Initialize()
    {
        if (m_Initialized) return;

        if (JPH::Factory::sInstance != nullptr)
            throw std::runtime_error("Jolt physics already initialized");

        // Must happen before using other jold functionality
        JPH::RegisterDefaultAllocator();
        JPH::Trace = JoltTraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl);
        JPH::Factory::sInstance = new JPH::Factory();

        try
        {
            JPH::RegisterTypes();
            constexpr std::uint32_t tempAllocatorSize = 10u * 1024u * 1024u;
            m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(tempAllocatorSize);
            m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, -1);

            m_Initialized = true;
            QUIET_CORE_INFO("Initialized Jolt Physics subsystem. maxConcurrency={}", m_JobSystem->GetMaxConcurrency());
        }
        catch (...)
        {
            m_JobSystem.reset();
            m_TempAllocator.reset();

            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
            JPH::Trace = nullptr;
            JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = nullptr);
            throw;
        }
    }

    void PhysicsSubsystem::Shutdown()
    {
        if (!m_Initialized) return;

        m_JobSystem.reset();
        m_TempAllocator.reset();

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
        JPH::Trace = nullptr;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = nullptr;)

        m_Initialized = false;

        QUIET_CORE_INFO("Shut down Jolt Physics subsystem.");
    }
}
