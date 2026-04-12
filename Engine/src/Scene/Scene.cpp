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

    void Scene::Clear()
    {
        m_Actors.clear();
    }

    void Scene::Update(float deltaTime)
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
