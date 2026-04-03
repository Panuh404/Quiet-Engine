#pragma once

#include <vector>
#include "Scene/Actor.h"

namespace Quiet::Scene
{
    class Scene
    {
    public:
        Scene();

        void Update(float deltaTime);

        const std::vector<Actor>& GetActors() const { return m_Actors; }
        std::vector<Actor>& GetActors() { return m_Actors; }
    private:
        std::vector<Actor> m_Actors;
    };
}