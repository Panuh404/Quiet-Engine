#pragma once

#include <cstddef>
#include <memory>

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Scene
{
    class Actor;
    class Scene;
}

namespace Quiet::Scripting
{
    class ScriptWorld
    {
    public:
        ScriptWorld(Scene::Scene& scene, const Project::ProjectContext& projectContext, const Input::Input& input);
        ~ScriptWorld();

        ScriptWorld(const ScriptWorld&) = delete;
        ScriptWorld& operator=(const ScriptWorld&) = delete;
        ScriptWorld(ScriptWorld&&) = delete;
        ScriptWorld& operator=(ScriptWorld&&) = delete;

        void BuildInstances();

        void Begin();
        void Update(float deltaTime);
        void DispatchPhysicsContactEvents();
        void End();

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        struct RuntimeState;

        void Initialize();
        void Shutdown();

        bool CreateInstance(const Scene::Actor& actor);
        void StartInstance(std::size_t index);
        void DestroyInstance(std::size_t index);
        void SynchronizeInstances();

    private:
        Scene::Scene& m_Scene;
        const Project::ProjectContext& m_ProjectContext;
        const Input::Input& m_Input;

        std::unique_ptr<RuntimeState> m_State;

        bool m_Initialized = false;
        bool m_Begun = false;
    };
}