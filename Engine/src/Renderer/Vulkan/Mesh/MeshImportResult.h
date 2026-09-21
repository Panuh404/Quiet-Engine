#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Quiet/Assets/MeshData.h"

namespace Quiet::Renderer::Vulkan
{
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

        [[nodiscard]] bool IsValid() const { return Index >= 0 && !Extension.empty() && !Bytes.empty(); }
    };

    struct MeshImportResult
    {
        std::vector<Assets::MeshVertex> Vertices;
        std::vector<Assets::MeshIndex> Indices;
        std::vector<ImportedSubmesh> Submeshes;
        std::vector<ImportedMaterial> Materials;
        std::vector<ImportedEmbeddedTexture> EmbeddedTextures;

        Assets::MeshBounds Bounds{};

        std::uint32_t ImportedMeshCount = 0;
        std::string ImporterName;
        std::string Error;
        std::vector<std::string> Warnings;

        [[nodiscard]] bool HasGeometry() const { return !Vertices.empty() && !Indices.empty(); }
        [[nodiscard]] bool Succeeded() const { return Error.empty() && HasGeometry(); }
    };
}