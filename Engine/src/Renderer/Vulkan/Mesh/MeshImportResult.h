#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "StaticMesh.h"

namespace Quiet::Renderer::Vulkan
{
    struct MeshBounds
    {
        glm::vec3 Min {0.0f };
        glm::vec3 Max {0.0f };
        bool Valid = false;
    };

    struct MeshImportResult
    {
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;

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