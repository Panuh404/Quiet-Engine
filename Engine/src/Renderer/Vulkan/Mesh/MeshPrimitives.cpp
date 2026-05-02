#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    glm::vec3 MakeColorFromPosition(const glm::vec3& position)
    {
        return {
            0.55f + 0.25f * (position.x >= 0.0f ? 1.0f : 0.0f),
            0.55f + 0.25f * (position.y >= 0.0f ? 1.0f : 0.0f),
            0.55f + 0.25f * (position.z >= 0.0f ? 1.0f : 0.0f)
        };
    }

    glm::vec3 SafeNormalize(const glm::vec3& value, const glm::vec3& fallback)
    {
        const float length = glm::length(value);
        if (length <= 0.00001f) return fallback;
        return value / length;
    }

    Quiet::Renderer::Vulkan::Vertex MakeVertex(const glm::vec3& position,
                                               const glm::vec3& normal,
                                               const glm::vec2& texCoord)
    {
        return {
            position,
            MakeColorFromPosition(position),
            SafeNormalize(normal, { 0.0f, 1.0f, 0.0f }),
            texCoord
        };
    }

    [[maybe_unused]]
    Quiet::Renderer::Vulkan::Vertex MakeVertex(const glm::vec3& position,
                                                                const glm::vec3& normal)
    {
        return MakeVertex(
            position,
            SafeNormalize(normal, { 0.0f, 1.0f, 0.0f }),
            { 0.0f, 0.0f }
        );
    }

    [[maybe_unused]]
    Quiet::Renderer::Vulkan::Vertex MakeVertex(const glm::vec3& position)
    {
        return MakeVertex(
            position,
            SafeNormalize(position, { 0.0f, 1.0f, 0.0f }),
            { 0.0f, 0.0f }
        );
    }

    Quiet::Renderer::Vulkan::MeshStats MakePrimitiveStats(
        const std::vector<Quiet::Renderer::Vulkan::Vertex>& vertices,
        const std::vector<Quiet::Renderer::Vulkan::Index>& indices)
    {
        Quiet::Renderer::Vulkan::MeshStats stats{};
        stats.VertexCount = static_cast<std::uint32_t>(vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(indices.size());
        stats.ImportedMeshCount = 1;
        stats.ImporterName = "Primitive";

        if (!vertices.empty())
        {
            stats.BoundsMin = vertices.front().Position;
            stats.BoundsMax = vertices.front().Position;
            stats.BoundsValid = true;

            for (const auto& vertex : vertices)
            {
                stats.BoundsMin = glm::min(stats.BoundsMin, vertex.Position);
                stats.BoundsMax = glm::max(stats.BoundsMax, vertex.Position);
            }
        }
        return stats;
    }

    Quiet::Renderer::Vulkan::MeshPrimitive MakePrimitive(
        std::vector<Quiet::Renderer::Vulkan::Vertex> vertices,
        std::vector<Quiet::Renderer::Vulkan::Index> indices)
    {
        Quiet::Renderer::Vulkan::MeshPrimitive primitive{};
        primitive.Vertices = std::move(vertices);
        primitive.Indices = std::move(indices);
        primitive.Stats = MakePrimitiveStats(primitive.Vertices, primitive.Indices);
        return primitive;
    }

    void AddQuadFace(std::vector<Quiet::Renderer::Vulkan::Vertex>& vertices,
                     std::vector<Quiet::Renderer::Vulkan::Index>& indices,
                     const glm::vec3& a,
                     const glm::vec3& b,
                     const glm::vec3& c,
                     const glm::vec3& d,
                     const glm::vec3& normal)
    {
        using Quiet::Renderer::Vulkan::Index;

        const auto start = static_cast<Index>(vertices.size());

        vertices.push_back(MakeVertex(a, normal, { 0.0f, 0.0f }));
        vertices.push_back(MakeVertex(b, normal, { 1.0f, 0.0f }));
        vertices.push_back(MakeVertex(c, normal, { 1.0f, 1.0f }));
        vertices.push_back(MakeVertex(d, normal, { 0.0f, 1.0f }));

        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);

        indices.push_back(start + 2);
        indices.push_back(start + 3);
        indices.push_back(start + 0);
    }

    void AddTriangleFace(std::vector<Quiet::Renderer::Vulkan::Vertex>& vertices,
                         std::vector<Quiet::Renderer::Vulkan::Index>& indices,
                         const glm::vec3& a,
                         const glm::vec3& b,
                         const glm::vec3& c,
                         const glm::vec2& uvA,
                         const glm::vec2& uvB,
                         const glm::vec2& uvC)
    {
        using Quiet::Renderer::Vulkan::Index;

        const auto start = static_cast<Index>(vertices.size());

        const glm::vec3 normal = SafeNormalize(
            glm::cross(b - a, c - a),
            { 0.0f, 1.0f, 0.0f }
        );

        vertices.push_back(MakeVertex(a, normal, uvA));
        vertices.push_back(MakeVertex(b, normal, uvB));
        vertices.push_back(MakeVertex(c, normal, uvC));

        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
    }
}

namespace Quiet::Renderer::Vulkan
{
    std::vector<std::string> MeshPrimitives::GetBuiltInAssetIDs()
    {
        return {
            "Triangle",
            "Quad",
            "Plane",
            "Cube",
            "Pyramid",
            "Sphere",
            "Cylinder"
        };
    }

    bool MeshPrimitives::IsBuiltInAssetID(std::string_view assetID)
    {
        const auto ids = GetBuiltInAssetIDs();
        return std::any_of(
             ids.begin(),
             ids.end(),
             [&](const std::string& id)
             {
                 return id == assetID;
             }
         );
    }

    std::optional<MeshPrimitive> MeshPrimitives::Create(const std::string_view assetID)
    {
        if (assetID == "Triangle")  return CreateTriangle();
        if (assetID == "Quad")      return CreateQuad();
        if (assetID == "Plane")     return CreatePlane();
        if (assetID == "Cube")      return CreateCube();
        if (assetID == "Pyramid")   return CreatePyramid();
        if (assetID == "Sphere")    return CreateSphere();
        if (assetID == "Cylinder")  return CreateCylinder();
        return std::nullopt;
    }

    #pragma region Primitive Definitions

    MeshPrimitive MeshPrimitives::CreateTriangle()
    {
        return MakePrimitive(
            // Vertices
            {
                MakeVertex({  0.0f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.5f, 1.0f }),
                MakeVertex({ -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }),
                MakeVertex({  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f })
            },
            // Indices
            {
                0, 1, 2
            }
        );
    }

    MeshPrimitive MeshPrimitives::CreateQuad()
    {
        return MakePrimitive(
           // Vertices
           {
               MakeVertex({ -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }),
               MakeVertex({  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }),
               MakeVertex({  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }),
               MakeVertex({ -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f })
           },
           // Indices
           {
               0, 1, 2,
               2, 3, 0
           }
       );
    }
    MeshPrimitive MeshPrimitives::CreatePlane()
    {
        return MakePrimitive(
           // Vertices
           {
               MakeVertex({ -0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }),
               MakeVertex({  0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }),
               MakeVertex({  0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }),
               MakeVertex({ -0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f })
           },
           // Indices
           {
               0, 1, 2,
               2, 3, 0
           }
       );
    }
    MeshPrimitive MeshPrimitives::CreateCube()
    {
        constexpr float h = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        vertices.reserve(24);
        indices.reserve(36);

        // Front +Z
        AddQuadFace(
            vertices,
            indices,
            { -h, -h,  h },
            {  h, -h,  h },
            {  h,  h,  h },
            { -h,  h,  h },
            { 0.0f, 0.0f, 1.0f }
        );

        // Back -Z
        AddQuadFace(
            vertices,
            indices,
            {  h, -h, -h },
            { -h, -h, -h },
            { -h,  h, -h },
            {  h,  h, -h },
            { 0.0f, 0.0f, -1.0f }
        );

        // Left -X
        AddQuadFace(
            vertices,
            indices,
            { -h, -h, -h },
            { -h, -h,  h },
            { -h,  h,  h },
            { -h,  h, -h },
            { -1.0f, 0.0f, 0.0f }
        );

        // Right +X
        AddQuadFace(
            vertices,
            indices,
            {  h, -h,  h },
            {  h, -h, -h },
            {  h,  h, -h },
            {  h,  h,  h },
            { 1.0f, 0.0f, 0.0f }
        );

        // Bottom -Y
        AddQuadFace(
            vertices,
            indices,
            { -h, -h, -h },
            {  h, -h, -h },
            {  h, -h,  h },
            { -h, -h,  h },
            { 0.0f, -1.0f, 0.0f }
        );

        // Top +Y
        AddQuadFace(
            vertices,
            indices,
            { -h,  h,  h },
            {  h,  h,  h },
            {  h,  h, -h },
            { -h,  h, -h },
            { 0.0f, 1.0f, 0.0f }
        );
        return MakePrimitive(std::move(vertices), std::move(indices));
    }
    MeshPrimitive MeshPrimitives::CreatePyramid()
    {
        constexpr float h = 0.5f;

        constexpr glm::vec3 baseFrontLeft{ -h, -h,  h };
        constexpr glm::vec3 baseFrontRight{ h, -h,  h };
        constexpr glm::vec3 baseBackRight{ h, -h, -h };
        constexpr glm::vec3 baseBackLeft{ -h, -h, -h };
        constexpr glm::vec3 tip{ 0.0f, h, 0.0f };

        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        vertices.reserve(18);
        indices.reserve(18);

        // Base -Y
        AddQuadFace(
            vertices,
            indices,
            baseBackLeft,
            baseBackRight,
            baseFrontRight,
            baseFrontLeft,
            { 0.0f, -1.0f, 0.0f }
        );

        // Front +Z
        AddTriangleFace(
            vertices,
            indices,
            baseFrontLeft,
            baseFrontRight,
            tip,
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.5f, 1.0f }
        );

        // Right +X
        AddTriangleFace(
            vertices,
            indices,
            baseFrontRight,
            baseBackRight,
            tip,
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.5f, 1.0f }
        );

        // Back -Z
        AddTriangleFace(
            vertices,
            indices,
            baseBackRight,
            baseBackLeft,
            tip,
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.5f, 1.0f }
        );

        // Left -X
        AddTriangleFace(
            vertices,
            indices,
            baseBackLeft,
            baseFrontLeft,
            tip,
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.5f, 1.0f }
        );

        return MakePrimitive(std::move(vertices), std::move(indices));
    }
    MeshPrimitive MeshPrimitives::CreateSphere()
    {
        constexpr std::uint32_t segments = 32;
        constexpr std::uint32_t rings = 16;
        constexpr float radius = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        vertices.reserve((segments + 1) * (rings + 1));

        for (std::uint32_t ring = 0; ring <= rings; ++ring)
        {
            const float v = static_cast<float>(ring) / static_cast<float>(rings);
            const float theta = v * kPi;

            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (std::uint32_t segment = 0; segment <= segments; ++segment)
            {
                const float u = static_cast<float>(segment) / static_cast<float>(segments);
                const float phi = u * kPi * 2.0f;

                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const glm::vec3 position{
                    radius * sinTheta * cosPhi,
                    radius * cosTheta,
                    radius * sinTheta * sinPhi
                };
                const glm::vec3 normal = SafeNormalize(position, { 0.0f, 1.0f, 0.0f });
                const glm::vec2 texCoord{ u, 1.0f - v };

                vertices.push_back(MakeVertex(position, normal, texCoord));
            }
        }

        for (std::uint32_t ring = 0; ring < rings; ++ring)
        {
            for (std::uint32_t segment = 0; segment < segments; ++segment)
            {
                const Index first = ring * (segments + 1) + segment;
                const Index second = first + segments + 1;

                if (ring != 0)
                {
                    indices.push_back(first);
                    indices.push_back(second);
                    indices.push_back(first + 1);
                }
                if (ring != rings - 1)
                {
                    indices.push_back(first + 1);
                    indices.push_back(second);
                    indices.push_back(second + 1);
                }
            }
        }

        return MakePrimitive(std::move(vertices), std::move(indices));
    }

    MeshPrimitive MeshPrimitives::CreateCylinder()
    {
        constexpr std::uint32_t segments = 32;
        constexpr float radius = 0.5f;
        constexpr float halfHeight = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        vertices.reserve((segments + 1) * 2 + 2 + segments * 2);
        indices.reserve(segments * 12);

        // Sides
        for (std::uint32_t segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float angle = u * kPi * 2.0f;

            const float x = std::cos(angle) * radius;
            const float z = std::sin(angle) * radius;

            const glm::vec3 normal = SafeNormalize(
                { std::cos(angle), 0.0f, std::sin(angle) },
                { 1.0f, 0.0f, 0.0f }
            );

            vertices.push_back(MakeVertex(
                { x, -halfHeight, z },
                normal,
                { u, 0.0f }
            ));

            vertices.push_back(MakeVertex(
                { x, halfHeight, z },
                normal,
                { u, 1.0f }
            ));
        }

        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const Index base = segment * 2;

            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);

            indices.push_back(base + 2);
            indices.push_back(base + 1);
            indices.push_back(base + 3);
        }

        // Bottom cap
        const auto bottomCenter = static_cast<Index>(vertices.size());
        vertices.push_back(MakeVertex(
            { 0.0f, -halfHeight, 0.0f },
            { 0.0f, -1.0f, 0.0f },
            { 0.5f, 0.5f }
        ));

        const auto bottomRingStart = static_cast<Index>(vertices.size());

        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float angle = u * kPi * 2.0f;

            const float cosAngle = std::cos(angle);
            const float sinAngle = std::sin(angle);

            vertices.push_back(MakeVertex(
                { cosAngle * radius, -halfHeight, sinAngle * radius },
                { 0.0f, -1.0f, 0.0f },
                { 0.5f + cosAngle * 0.5f, 0.5f + sinAngle * 0.5f }
            ));
        }

        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const Index current = bottomRingStart + segment;
            const Index next = bottomRingStart + ((segment + 1) % segments);

            indices.push_back(bottomCenter);
            indices.push_back(current);
            indices.push_back(next);
        }

        // Top cap
        const auto topCenter = static_cast<Index>(vertices.size());
        vertices.push_back(MakeVertex(
            { 0.0f, halfHeight, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f }
        ));

        const auto topRingStart = static_cast<Index>(vertices.size());

        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float angle = u * kPi * 2.0f;

            const float cosAngle = std::cos(angle);
            const float sinAngle = std::sin(angle);

            vertices.push_back(MakeVertex(
                { cosAngle * radius, halfHeight, sinAngle * radius },
                { 0.0f, 1.0f, 0.0f },
                { 0.5f + cosAngle * 0.5f, 0.5f + sinAngle * 0.5f }
            ));
        }

        for (std::uint32_t segment = 0; segment < segments; ++segment)
        {
            const Index current = topRingStart + segment;
            const Index next = topRingStart + ((segment + 1) % segments);

            indices.push_back(topCenter);
            indices.push_back(current);
            indices.push_back(next);
        }

        return MakePrimitive(std::move(vertices), std::move(indices));
    }


#pragma endregion
}