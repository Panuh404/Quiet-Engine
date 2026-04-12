#include "Scene.h"

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
        actor.ID = Quiet::Core::UUIDGenerator::New();
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

    Actor* Scene::TryDuplicateActor(Core::UUID actorID)
    {
        Actor* source = FindActorByID(actorID);
        if (!source)
        {
            return nullptr;
        }

        Actor duplicate = *source;
        duplicate.ID = Core::UUID();
        duplicate.Name += " Copy";

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
    }

    void Scene::Update(float deltaTime, bool runRuntimeSystems)
    {
        if (!runRuntimeSystems) return;
        
        for (auto& actor : m_Actors)
        {
            if (actor.Rotator.has_value())
            {
                actor.Transform.WorldTransform.RotationDegrees.z +=
                    actor.Rotator->RotationSpeedDegreesPerSecond * deltaTime;
            }
        }
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
}
