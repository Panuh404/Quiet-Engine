#include "Scene.h"

#include "Quiet/Core/Log.h"

namespace Quiet::Scene
{
    Scene::Scene()
    {
        auto& center = CreateActor("Center");
        center.Transform.WorldTransform.Position = { 0.0f, 0.0f, 0.0f };
        center.Transform.WorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
        center.Rotator = RotatorComponent{ 0.0f };

        auto& left = CreateActor("Left");
        left.Transform.WorldTransform.Position = { -1.5f, 0.0f, 0.0f };
        left.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        left.Rotator = RotatorComponent{ 45.0f };

        auto& right = CreateActor("Right");
        right.Transform.WorldTransform.Position = { 1.5f, 0.0f, 0.0f };
        right.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        right.Rotator = RotatorComponent{ -60.0f };

        QUIET_CORE_INFO("Initialized scene with {} actors.", m_Actors.size());
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

    void Scene::Clear()
    {
        m_Actors.clear();
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
