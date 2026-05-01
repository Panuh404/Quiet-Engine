#pragma once

#include <vector>

#include "MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"

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
        [[nodiscard]] static std::vector<std::string> GetBuiltInAssetIDs();
        [[nodiscard]] static bool IsBuiltInAssetID(std::string_view assetID);
        [[nodiscard]] static std::optional<MeshPrimitive> Create(std::string_view assetID);

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