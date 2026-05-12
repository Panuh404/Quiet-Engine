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

        std::string BaseColorTexturePath;
        std::string BaseColorTextureAssetID;

        bool BaseColorTextureEmbedded = false;
        int EmbeddedTextureIndex = -1;
    };

    struct ImportedEmbeddedTexture
    {
        int Index = -1;

        std::string Name;
        std::string Extension;

        std::vector<std::uint8_t> Bytes;

        [[nodiscard]] bool IsValid() const
        {
            return Index >= 0 &&
                   !Extension.empty() &&
                   !Bytes.empty();
        }
    };

    struct MeshImportResult
    {
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;
        std::vector<ImportedSubmesh> Submeshes;
        std::vector<ImportedMaterial> Materials;
        std::vector<ImportedEmbeddedTexture> EmbeddedTextures;

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