#pragma once

#include <memory>
#include <string>

namespace Quiet::Renderer::Vulkan
{
    class StaticMesh;
}

namespace Quiet::Scene
{
    struct StaticMeshComponent
    {
        std::string MeshResourceID;
        std::shared_ptr<Renderer::Vulkan::StaticMesh> Mesh;
        bool Visible = true;

        [[nodiscard]] bool IsValid() const { return Mesh != nullptr; }
    };
}