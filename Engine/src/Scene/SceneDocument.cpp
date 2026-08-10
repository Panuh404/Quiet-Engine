#include "Quiet/Scene/SceneDocument.h"

#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"

namespace Quiet::Scene
{
    void SceneDocument::CreateDefault(Scene& scene, const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        scene.Clear();

        // Cube
        auto& center = scene.CreateActor("Cube");
        auto& centerTransform = center.GetComponent<TransformComponent>();
        centerTransform.WorldTransform.Position = { 0.0f, 0.0f, 0.0f };
        centerTransform.WorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
        center.AddComponent<RotatorComponent>(RotatorComponent{ 0.0f });
        auto& centerMesh = center.AddComponent<StaticMeshComponent>();
        centerMesh.MeshResourceID = "Cube";
        centerMesh.Mesh = meshLibrary.Get("Cube");
        centerMesh.Visible = true;

        // Camera
        auto& camera = scene.CreateActor("Camera");
        auto& cameraTransform = camera.GetComponent<TransformComponent>();
        cameraTransform.WorldTransform.Position = { 0.0f, 0.0f, 6.0f };
        cameraTransform.WorldTransform.RotationDegrees = { 0.0f, 0.0f, 0.0f };
        auto& cameraComponent = camera.AddComponent<CameraComponent>();
        cameraComponent.Primary = true;
        cameraComponent.FieldOfViewDegrees = 60.0f;
        cameraComponent.NearClip = 0.1f;
        cameraComponent.FarClip = 1000.0f;
    }

    bool SceneDocument::Save(const Scene& scene, const std::filesystem::path& path)
    {
        return SceneSerializer::SaveToFile(scene, path);
    }

    bool SceneDocument::Load(Scene& scene, const Renderer::Vulkan::MeshLibrary& meshLibrary, const std::filesystem::path& path)
    {
        return SceneSerializer::LoadFromFile(scene, meshLibrary, path);
    }
}