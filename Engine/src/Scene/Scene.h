#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Scene/Actor.h"
#include "Scene/SceneHierarchy.h"
#include "Scene/SceneRenderSettings.h"

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
        bool MoveActor(Core::UUID actorID, std::size_t destinationIndex);

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

        [[nodiscard]] SceneHierarchyState& GetHierarchy() { return m_Hierarchy; }
        [[nodiscard]] const SceneHierarchyState& GetHierarchy() const { return m_Hierarchy; }

        void BeginRuntime(RuntimeMode mode);
        void EndRuntime();
        void HandleRuntimeInput(const Input::Input& input, float deltaTime);
        void Update(float deltaTime);

        [[nodiscard]] bool IsRuntimeActive() const { return m_RuntimeMode != RuntimeMode::None; }
        [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_RuntimeMode; }

        [[nodiscard]] std::vector<Actor>& GetActors() { return m_Actors; }
        [[nodiscard]] const std::vector<Actor>& GetActors() const { return m_Actors; }

        [[nodiscard]] SceneRenderSettings& GetRenderSettings() { return m_RenderSettings; }
        [[nodiscard]] const SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }

        [[nodiscard]] Actor* FindActorByID(Core::UUID id);
        [[nodiscard]] const Actor* FindActorByID(Core::UUID id) const;

        [[nodiscard]] Actor* FindPrimaryCameraActor();
        [[nodiscard]] const Actor* FindPrimaryCameraActor() const;

    private:
        void AddActorToHierarchyRoot(Core::UUID actorID);
        void RemoveActorFromHierarchy(Core::UUID actorID);
        bool MoveActorHierarchyRootNode(Core::UUID actorID, std::size_t destinationIndex);

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
        SceneRenderSettings m_RenderSettings{};
        std::vector<Actor> m_Actors;
        RuntimeMode m_RuntimeMode = RuntimeMode::None;
        SceneHierarchyState m_Hierarchy{};
    };
}