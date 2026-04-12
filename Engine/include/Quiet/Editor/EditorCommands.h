#pragma once
#include <string>

#include "Quiet/Core/UUID.h"
#include "glm/vec3.hpp"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
}

namespace Quiet::Editor
{
    class EditorController;

    class EditorCommands
    {
    public:
        explicit EditorCommands(EditorController& editorController);

        [[nodiscard]] bool CanEditScene() const;

        void BeginInteraction(const Scene::Scene& scene);
        bool EndInteraction(const Scene::Scene& scene);

        bool CreateEmptyActor(Scene::Scene& scene, const std::string& name = "Actor");
        bool DuplicateSelectedActor(Scene::Scene& scene);
        bool DeleteSelectedActor(Scene::Scene& scene);

        bool DuplicateActor(Scene::Scene& scene, Core::UUID actorID);
        bool DeleteActor(Scene::Scene& scene, Core::UUID actorID);

        bool RenameActor(Scene::Scene& scene, Core::UUID actorID, const std::string& newName);
        bool SetActorPosition(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& position);
        bool SetActorRotation(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& rotationDegrees);
        bool SetActorScale(Scene::Scene& scene, Core::UUID actorID, const glm::vec3& scale);

        bool AddStaticMesh(Scene::Scene& scene,
                           Core::UUID actorID,
                           const Renderer::Vulkan::MeshLibrary& meshLibrary);

        bool RemoveStaticMesh(Scene::Scene& scene, Core::UUID actorID);

        bool SetStaticMeshResource(Scene::Scene& scene,
                                   Core::UUID actorID,
                                   const std::string& meshResourceID,
                                   const Renderer::Vulkan::MeshLibrary& meshLibrary);

        bool SetStaticMeshVisible(Scene::Scene& scene,
                                  Core::UUID actorID,
                                  bool visible);

        bool AddRotator(Scene::Scene& scene, Core::UUID actorID, float initialSpeed = 45.0f);
        bool RemoveRotator(Scene::Scene& scene, Core::UUID actorID);
        bool SetRotatorSpeed(Scene::Scene& scene, Core::UUID actorID, float speed);

    private:
        EditorController& m_EditorController;
    };
}