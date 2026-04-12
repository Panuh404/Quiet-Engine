#pragma once

#include <vector>
#include "Scene/Actor.h"

namespace Quiet::Scene
{
    class Scene
    {
    public:
        Scene();
        ~Scene() = default;

        Actor& CreateActor(const std::string& name);
        Actor& CreateActorWithID(Core::UUID id, const std::string& name);

        Actor& DuplicateActor(Core::UUID sourceID);
        Actor* TryDuplicateActor(Core::UUID sourceID);

        bool DestroyActorByID(Core::UUID id);

        void Clear();
        void Update(float deltaTime, bool runRuntimeSystems);

        [[nodiscard]] std::vector<Actor>& GetActors() { return m_Actors; }
        [[nodiscard]] const std::vector<Actor>& GetActors() const { return m_Actors; }

        [[nodiscard]] Actor* FindActorByID(Quiet::Core::UUID id);
        [[nodiscard]] const Actor* FindActorByID(Quiet::Core::UUID id) const;
    private:
        std::vector<Actor> m_Actors;
    };
}