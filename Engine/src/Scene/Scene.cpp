#include "Scene.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"

namespace Quiet::Scene
{
    Scene::Scene()
    {
        QUIET_CORE_INFO("Initialized scene");
    }

    Actor& Scene::CreateActor(const std::string &name)
    {
        Actor actor{};
        actor.ID = Core::UUIDGenerator::New();
        actor.Name = name;

        m_Actors.push_back(std::move(actor));
        return m_Actors.back();
    }

    Actor& Scene::CreateActorWithID(Core::UUID id, const std::string& name)
    {
        Actor actor{};
        actor.ID = id;
        actor.Name = name;
        
        m_Actors.push_back(std::move(actor));
        return m_Actors.back();
    }

    Actor& Scene::DuplicateActor(Core::UUID sourceID)
    {
        Actor* duplicate = TryDuplicateActor(sourceID);
        if (!duplicate)
        {
            QUIET_CORE_ERROR("Failed to duplicate actor. Actor not found.");
            throw std::runtime_error("Scene::DuplicateActor failed. Source actor not found.");
        }

        return *duplicate;
    }

    Actor* Scene::TryDuplicateActor(Core::UUID sourceID)
    {
        const Actor* source = FindActorByID(sourceID);
        if (!source)
        {
            QUIET_CORE_WARN("TryDuplicateActor failed. Source actor not found.");
            return nullptr;
        }

        Actor duplicate = *source;
        duplicate.ID = Quiet::Core::UUIDGenerator::New();
        duplicate.Name = source->Name.empty() ? "Actor Copy" : source->Name + " Copy";

        m_Actors.push_back(std::move(duplicate));
        return &m_Actors.back();
    }

    bool Scene::DestroyActorByID(Core::UUID id)
    {
        const auto it = std::find_if(
            m_Actors.begin(),
            m_Actors.end(),
            [id](const Actor& actor)
            {
                return actor.ID == id;
            }
        );

        if (it == m_Actors.end())
            return false;

        m_Actors.erase(it);
        return true;
    }

    void Scene::Clear()
    {
        m_Actors.clear();
        m_RuntimeMode = RuntimeMode::None;
    }

    void Scene::BeginRuntime(RuntimeMode mode)
    {
        if (mode == RuntimeMode::None)
        {
            QUIET_CORE_WARN("Scene::BeginRuntime called with RuntimeMode::None.");
            return;
        }

        if (m_RuntimeMode != RuntimeMode::None)
        {
            QUIET_CORE_WARN("Scene runtime already active. Restarting runtime mode.");
            EndRuntime();
        }
        m_RuntimeMode = mode;
        OnRuntimeBegin(mode);
    }

    void Scene::EndRuntime()
    {
        if (m_RuntimeMode == RuntimeMode::None) return;
        const RuntimeMode previousMode = m_RuntimeMode;
        OnRuntimeEnd(previousMode);
        m_RuntimeMode = RuntimeMode::None;
    }

    void Scene::HandleRuntimeInput(const Input::Input& input, float deltaTime)
    {
        if (!IsRuntimeActive()) return;

        switch (m_RuntimeMode)
        {
            case RuntimeMode::Simulate:
                HandleSimulateInput(input, deltaTime);
                break;

            case RuntimeMode::Play:
                HandlePlayInput(input, deltaTime);
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::Update(float deltaTime)
    {
        if (!IsRuntimeActive()) return;
        UpdateRuntime(deltaTime);
    }

    Actor* Scene::FindActorByID(Core::UUID id)
    {
        for (auto& actor : m_Actors)
        {
            if (actor.ID == id) return &actor;
        }
        return nullptr;
    }

    const Actor* Scene::FindActorByID(Core::UUID id) const
    {
        for (const auto& actor : m_Actors)
        {
            if (actor.ID == id) return &actor;
        }
        return nullptr;
    }

    void Scene::OnRuntimeBegin(RuntimeMode mode)
    {
        switch (mode)
        {
            case RuntimeMode::Simulate:
                QUIET_CORE_INFO("Scene runtime begin: Simulate");
                OnSimulateBegin();
                break;

            case RuntimeMode::Play:
                QUIET_CORE_INFO("Scene runtime begin: Play");
                OnPlayBegin();
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::OnRuntimeEnd(RuntimeMode mode)
    {
        switch (mode)
        {
            case RuntimeMode::Simulate:
                QUIET_CORE_INFO("Scene runtime end: Simulate");
                OnSimulateEnd();
                break;

            case RuntimeMode::Play:
                QUIET_CORE_INFO("Scene runtime end: Play");
                OnPlayEnd();
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::OnSimulateBegin()
    {

    }

    void Scene::OnSimulateEnd()
    {

    }

    void Scene::OnPlayBegin()
    {

    }

    void Scene::OnPlayEnd()
    {

    }

    void Scene::HandleSimulateInput(const Input::Input &input, float deltaTime)
    {
        (void)input;
        (void)deltaTime;
    }

    void Scene::HandlePlayInput(const Input::Input &input, float deltaTime)
    {
        (void)input;
        (void)deltaTime;
    }

    void Scene::UpdateRuntime(float deltaTime)
    {
        for (auto& actor : m_Actors)
        {
            if (actor.Rotator.has_value())
            {
                actor.Transform.WorldTransform.RotationDegrees.z +=
                    actor.Rotator->RotationSpeedDegreesPerSecond * deltaTime;
            }
        }
    }

}
