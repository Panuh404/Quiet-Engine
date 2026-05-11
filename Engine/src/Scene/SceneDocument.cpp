#include "Quiet/Scene/SceneDocument.h"

#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Components/RotatorComponent.h"
#include "Scene/Components/StaticMeshComponent.h"

namespace Quiet::Scene
{
    void SceneDocument::CreateDefault(Scene& scene,
                                      const Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        scene.Clear();

        auto& center = scene.CreateActor("Center");
        center.Transform.WorldTransform.Position = { 0.0f, 0.0f, 0.0f };
        center.Transform.WorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
        center.Rotator = RotatorComponent{ 0.0f };
        center.StaticMesh = StaticMeshComponent{};
        center.StaticMesh->MeshResourceID = "Cube";
        center.StaticMesh->Mesh = meshLibrary.Get("Cube");
        center.StaticMesh->Visible = true;

        auto& left = scene.CreateActor("Left");
        left.Transform.WorldTransform.Position = { -2.0f, 0.0f, 0.0f };
        left.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        left.Rotator = RotatorComponent{ 45.0f };
        left.StaticMesh = StaticMeshComponent{};
        left.StaticMesh->MeshResourceID = "Quad";
        left.StaticMesh->Mesh = meshLibrary.Get("Quad");
        left.StaticMesh->Visible = true;

        auto& right = scene.CreateActor("Right");
        right.Transform.WorldTransform.Position = { 2.0f, 0.0f, -1.0f };
        right.Transform.WorldTransform.Scale = { 0.75f, 0.75f, 0.75f };
        right.Rotator = RotatorComponent{ -60.0f };
        right.StaticMesh = StaticMeshComponent{};
        right.StaticMesh->MeshResourceID = "Cube";
        right.StaticMesh->Mesh = meshLibrary.Get("Cube");
        right.StaticMesh->Visible = true;

        auto& camera = scene.CreateActor("Camera");
        camera.Transform.WorldTransform.Position = { 0.0f, 0.0f, 6.0f };
        camera.Transform.WorldTransform.RotationDegrees = { 0.0f, 0.0f, 0.0f };
        camera.Camera = CameraComponent{};
        camera.Camera->Primary = true;
        camera.Camera->FieldOfViewDegrees = 60.0f;
        camera.Camera->NearClip = 0.1f;
        camera.Camera->FarClip = 1000.0f;
    }

    bool SceneDocument::Save(const Scene& scene, const std::filesystem::path& path)
    {
        return SceneSerializer::SaveToFile(scene, path);
    }

    bool SceneDocument::Load(Scene& scene,
                             const Renderer::Vulkan::MeshLibrary& meshLibrary,
                             const std::filesystem::path& path)
    {
        return SceneSerializer::LoadFromFile(scene, meshLibrary, path);
    }
}