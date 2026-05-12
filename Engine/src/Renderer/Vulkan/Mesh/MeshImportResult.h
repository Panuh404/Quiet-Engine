#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Renderer/Vulkan/Mesh/StaticMesh.h"

namespace Quiet::Renderer::Vulkan
{
    struct MeshBounds
    {
        glm::vec3 Min {0.0f };
        glm::vec3 Max {0.0f };
        bool Valid = false;
    };

    struct ImportedSubmesh
    {
        std::string Name;

        std::uint32_t BaseVertex = 0;
        std::uint32_t VertexCount = 0;

        std::uint32_t BaseIndex = 0;
        std::uint32_t IndexCount = 0;

        std::uint32_t MaterialIndex = 0;
    };

    struct ImportedMaterial
    {
        std::string Name;

        glm::vec4 BaseColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Raw source texture reference reported by Assimp.
        // For external textures this is usually a relative path.
        // For embedded GLB textures this is often something like "*0".
        std::string BaseColorTexturePath;

        bool BaseColorTextureEmbedded = false;
        int EmbeddedTextureIndex = -1;
    };

    struct MeshImportResult
    {
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;
        std::vector<ImportedSubmesh> Submeshes;
        std::vector<ImportedMaterial> Materials;

        MeshBounds Bounds{};

        std::uint32_t ImportedMeshCount = 0;
        std::string ImporterName;
        std::string Error;
        std::vector<std::string> Warnings;

        [[nodiscard]] bool HasGeometry() const
        {
            return !Vertices.empty() && !Indices.empty();
        }
        [[nodiscard]] bool Succeeded() const
        {
            return Error.empty() && HasGeometry();
        }
    };
}