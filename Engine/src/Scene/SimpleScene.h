#pragma once

#include <vector>
#include "Scene/SceneObject.h"

namespace Quiet::Scene
{
    class SimpleScene
    {
    public:
        SimpleScene();

        void Update(float deltaTime);

        const std::vector<SceneObject>& GetObjects() const { return m_Objects; }
    private:
        std::vector<SceneObject> m_Objects;
    };
}