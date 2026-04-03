#include "SimpleScene.h"

namespace Quiet::Scene
{
    SimpleScene::SimpleScene()
    {
        SceneObject center{};
        center.TransformData.Position = { 0.0f, 0.0f, 0.0f };
        center.TransformData.Scale = { 1.0f, 1.0f, 1.0f };
        center.RotationSpeedDegreesPerSecond = 0.0f;
        m_Objects.push_back(center);

        SceneObject left{};
        left.TransformData.Position = { -1.5f, 0.0f, 0.0f };
        left.TransformData.Scale = { 0.75f, 0.75f, 0.75f };
        left.RotationSpeedDegreesPerSecond = 45.0f;
        m_Objects.push_back(left);

        SceneObject right{};
        right.TransformData.Position = { 1.5f, 0.0f, 0.0f };
        right.TransformData.Scale = { 0.75f, 0.75f, 0.75f };
        right.RotationSpeedDegreesPerSecond = -60.0f;
        m_Objects.push_back(right);
    }

    void SimpleScene::Update(float deltaTime)
    {
        for (auto& object : m_Objects)
        {
            object.TransformData.RotationDegrees.z +=
                object.RotationSpeedDegreesPerSecond * deltaTime;
        }
    }
}
