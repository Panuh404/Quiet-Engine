#pragma once

#include <memory>

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Scripting
{
    class ScriptWorld
    {
    public:
        ScriptWorld(Scene::Scene& scene, const Project::ProjectContext& projectContext);
        ~ScriptWorld();

        ScriptWorld(const ScriptWorld&) = delete;
        ScriptWorld& operator=(const ScriptWorld&) = delete;
        ScriptWorld(ScriptWorld&&) = delete;
        ScriptWorld& operator=(ScriptWorld&&) = delete;

        void BuildInstances();

        void Begin();
        void Update(float deltaTime);
        void End();

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        struct RuntimeState;

        void Initialize();
        void Shutdown();

    private:
        Scene::Scene& m_Scene;
        const Project::ProjectContext& m_ProjectContext;

        std::unique_ptr<RuntimeState> m_State;

        bool m_Initialized = false;
        bool m_Begun = false;
    };
}