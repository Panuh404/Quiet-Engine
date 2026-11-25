#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Quiet/Core/UUID.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"

namespace Quiet::Renderer::Vulkan
{
    struct MeshPrimitive
    {
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;
        MeshStats Stats;

        [[nodiscard]] bool IsValid() const { return !Vertices.empty() && !Indices.empty(); }
    };

    class MeshPrimitives
    {
    public:
        [[nodiscard]] static std::vector<Core::UUID> GetBuiltInAssetIDs();
        [[nodiscard]] static bool IsBuiltInAssetID(Core::UUID assetID);
        [[nodiscard]] static std::optional<MeshPrimitive> Create(Core::UUID assetID);

    private:
        [[nodiscard]] static MeshPrimitive CreateTriangle();
        [[nodiscard]] static MeshPrimitive CreateQuad();
        [[nodiscard]] static MeshPrimitive CreatePlane();
        [[nodiscard]] static MeshPrimitive CreateCube();
        [[nodiscard]] static MeshPrimitive CreatePyramid();
        [[nodiscard]] static MeshPrimitive CreateSphere();
        [[nodiscard]] static MeshPrimitive CreateCylinder();
    };
}