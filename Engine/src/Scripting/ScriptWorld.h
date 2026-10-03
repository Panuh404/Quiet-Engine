#pragma once

#include <cstddef>
#include <memory>
#include <filesystem>
#include <string_view>

#include "Quiet/Core/UUID.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Project
{
    class ProjectContext;
    struct RuntimeManifest;
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
        ScriptWorld(Scene::Scene& scene,
                    const Project::ProjectContext& projectContext,
                    const Input::Input& input,
                    std::filesystem::path runtimePackageRoot = {},
                    const Project::RuntimeManifest* runtimeManifest = nullptr);
        ~ScriptWorld();

        ScriptWorld(const ScriptWorld&) = delete;
        ScriptWorld& operator=(const ScriptWorld&) = delete;
        ScriptWorld(ScriptWorld&&) = delete;
        ScriptWorld& operator=(ScriptWorld&&) = delete;

        void BuildInstances();

        void Begin();
        void Update(float deltaTime);

        void DispatchPhysicsContactEvents();
        void NotifyActorWillBeDestroyed(Core::UUID actorID);

        void End();

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        struct RuntimeState;

        void Initialize();
        void Shutdown();

        bool ResolveScriptPath(std::string_view scriptAssetID, std::filesystem::path& outPath, std::string& outError) const;

        bool CreateInstance(const Scene::Actor& actor, bool recordFailure = true);
        void StartInstance(std::size_t index);
        void DestroyInstance(std::size_t index);
        void SynchronizeInstances();
        void ReloadChangedInstances();

    private:
        Scene::Scene& m_Scene;
        const Project::ProjectContext& m_ProjectContext;
        const Input::Input& m_Input;
        std::filesystem::path m_RuntimePackageRoot;
        std::unique_ptr<Project::RuntimeManifest> m_RuntimeManifest;

        std::unique_ptr<RuntimeState> m_State;

        bool m_Initialized = false;
        bool m_Begun = false;
    };
}