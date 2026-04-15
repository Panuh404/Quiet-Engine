#pragma once

#include <vector>
#include "Scene/Actor.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    enum class RuntimeMode
    {
        None,
        Simulate,
        Play
    };

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

        void BeginRuntime(RuntimeMode mode);
        void EndRuntime();
        void HandleRuntimeInput(const Input::Input& input, float deltaTime);
        void Update(float deltaTime);

        [[nodiscard]] bool IsRuntimeActive() const { return m_RuntimeMode != RuntimeMode::None; }
        [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_RuntimeMode; }

        [[nodiscard]] std::vector<Actor>& GetActors() { return m_Actors; }
        [[nodiscard]] const std::vector<Actor>& GetActors() const { return m_Actors; }

        [[nodiscard]] Actor* FindActorByID(Quiet::Core::UUID id);
        [[nodiscard]] const Actor* FindActorByID(Quiet::Core::UUID id) const;

    private:
        void OnRuntimeBegin(RuntimeMode mode);
        void OnRuntimeEnd(RuntimeMode mode);
        void OnSimulateBegin();
        void OnSimulateEnd();
        void OnPlayBegin();
        void OnPlayEnd();

        void HandleSimulateInput(const Input::Input& input, float deltaTime);
        void HandlePlayInput(const Input::Input& input, float deltaTime);
        void UpdateRuntime(float deltaTime);

    private:
        std::vector<Actor> m_Actors;
        RuntimeMode m_RuntimeMode = RuntimeMode::None;
    };
}