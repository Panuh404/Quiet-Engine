#pragma once
#include <memory>

namespace Quiet::Renderer::Vulkan
{
    class StaticMesh;
}

namespace Quiet::Scene
{
    struct StaticMeshComponent
    {
        std::shared_ptr<Quiet::Renderer::Vulkan::StaticMesh> Mesh;
        bool Visible = true;

        [[nodiscard]] bool IsValid() const { return Mesh != nullptr; }
    };
}