#include "Scene.h"

namespace Quiet::Scene
{
    Scene::Scene()
    {
        Actor center{};
        center.Transform.WorldTransform.Position = { 0.0f, 0.0f, 0.0f };
        center.Transform.WorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
        center.Rotator = RotatorComponent{ 0.0f };
        m_Actors.push_back(center);

        Actor left{};
        left.Transform.WorldTransform.Position = { -1.5f, 0.0f, 0.0f };
        left.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        left.Rotator = RotatorComponent{ 45.0f };
        m_Actors.push_back(left);

        Actor right{};
        right.Transform.WorldTransform.Position = { 1.5f, 0.0f, 0.0f };
        right.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        right.Rotator = RotatorComponent{ -60.0f };
        m_Actors.push_back(right);
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
}
