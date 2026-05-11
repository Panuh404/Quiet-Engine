#pragma once

#include <memory>
#include <string>

#include "Scene/Components/MaterialComponent.h"

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
        std::vector<MaterialComponent> MaterialSlots;

        bool Visible = true;

        [[nodiscard]] bool IsValid() const { return Mesh != nullptr; }

        [[nodiscard]] const MaterialComponent* TryGetMaterialSlot(const std::uint32_t slot) const
        {
            if (slot >= MaterialSlots.size())
                return nullptr;
            return &MaterialSlots[slot];
        }

        [[nodiscard]] MaterialComponent* TryGetMaterialSlot(const std::uint32_t slot)
        {
            if (slot >= MaterialSlots.size())
                return nullptr;
            return &MaterialSlots[slot];
        }
    };
}