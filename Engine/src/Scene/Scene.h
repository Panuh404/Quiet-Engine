#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "Scene/Actor.h"
#include "Scene/SceneHierarchy.h"
#include "Scene/SceneRenderSettings.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem;
    class PhysicsWorld;
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
        Scene(const Scene& other);
        Scene& operator=(const Scene& other);
        ~Scene();

        Actor& CreateActor(const std::string& name);
        Actor& CreateActorWithID(Core::UUID id, const std::string& name);

        Actor& DuplicateActor(Core::UUID sourceID);
        Actor* TryDuplicateActor(Core::UUID sourceID);

        bool DestroyActorByID(Core::UUID id);
        void Clear();

        void ResetHierarchyFromActors();
        void SetHierarchy(SceneHierarchyState hierarchy);

        Core::UUID CreateHierarchyFolder(const std::string& name = "Folder");

        [[nodiscard]] SceneHierarchyFolder* FindHierarchyFolder(Core::UUID folderID);
        [[nodiscard]] const SceneHierarchyFolder* FindHierarchyFolder(Core::UUID folderID) const;

        bool RenameHierarchyFolder(Core::UUID folderID, const std::string& newName);
        bool DeleteEmptyHierarchyFolder(Core::UUID folderID);

        bool MoveActorToHierarchyFolder(Core::UUID actorID, Core::UUID folderID);
        bool MoveActorToHierarchyRoot(Core::UUID actorID);
        bool MoveHierarchyRootNode(SceneHierarchyNodeType nodeType, Core::UUID nodeID, std::size_t destinationIndex);

        [[nodiscard]] SceneHierarchyState& GetHierarchy() { return m_Hierarchy; }
        [[nodiscard]] const SceneHierarchyState& GetHierarchy() const { return m_Hierarchy; }

        void BeginRuntime(RuntimeMode mode, Physics::PhysicsSubsystem& physicsSubsystem);
        void EndRuntime();
        void HandleRuntimeInput(const Input::Input& input, float deltaTime);
        void Update(float deltaTime);

        [[nodiscard]] bool IsRuntimeActive() const { return m_RuntimeMode != RuntimeMode::None; }
        [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_RuntimeMode; }

        [[nodiscard]] std::vector<Actor>& GetActors() { return m_Actors; }
        [[nodiscard]] const std::vector<Actor>& GetActors() const { return m_Actors; }
        [[nodiscard]] const entt::registry& GetRegistry() const { return m_Registry; }

        [[nodiscard]] SceneRenderSettings& GetRenderSettings() { return m_RenderSettings; }
        [[nodiscard]] const SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }

        [[nodiscard]] Actor* FindActorByID(Core::UUID id);
        [[nodiscard]] const Actor* FindActorByID(Core::UUID id) const;
        [[nodiscard]] entt::entity FindEntityByID(Core::UUID id) const;

        [[nodiscard]] Actor* FindPrimaryCameraActor();
        [[nodiscard]] const Actor* FindPrimaryCameraActor() const;
        [[nodiscard]] const DirectionalLightComponent* FindActiveDirectionalLight() const;

    private:
        void AddActorToHierarchyRoot(Core::UUID actorID);
        void RemoveActorFromHierarchy(Core::UUID actorID);
        void SanitizeHierarchy();

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
        entt::registry m_Registry;
        SceneRenderSettings m_RenderSettings{};
        std::vector<Actor> m_Actors;
        RuntimeMode m_RuntimeMode = RuntimeMode::None;
        SceneHierarchyState m_Hierarchy{};
        std::unique_ptr<Physics::PhysicsWorld> m_PhysicsWorld;
    };
}