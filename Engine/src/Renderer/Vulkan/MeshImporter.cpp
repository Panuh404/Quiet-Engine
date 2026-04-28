#include "MeshImporter.h"

#include <limits>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace
{
    glm::vec3 ToVec3(const aiVector3D& value)
    {
        return { value.x, value.y, value.z };
    }

    glm::vec3 MakeDefaultColor(const glm::vec3& position)
    {
        return {
            0.55f + 0.25f * (position.x >= 0.0f ? 1.0f : 0.0f),
            0.55f + 0.25f * (position.y >= 0.0f ? 1.0f : 0.0f),
            0.55f + 0.25f * (position.z >= 0.0f ? 1.0f : 0.0f)
        };
    }

    glm::vec3 GetVertexColor(const aiMesh& mesh,
                             const unsigned int vertexIndex,
                             const glm::vec3& fallbackPosition)
    {
        if (mesh.HasVertexColors(0) && mesh.mColors[0])
        {
            const aiColor4D& color = mesh.mColors[0][vertexIndex];
            return { color.r, color.g, color.b };
        }
        return MakeDefaultColor(fallbackPosition);
    }
}

namespace Quiet::Renderer::Vulkan
{
    bool MeshImporter::LoadFromFile(const std::filesystem::path &path,
                                       std::vector<Vertex> &outVertices,
                                       std::vector<std::uint16_t> &outIndices,
                                       std::string &outError)
    {
        outVertices.clear();
        outIndices.clear();
        outError.clear();

        Assimp::Importer importer;
        constexpr unsigned int importFlags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_ImproveCacheLocality |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_FindInvalidData |
            aiProcess_SortByPType;

        const aiScene* scene = importer.ReadFile(path.string(), importFlags);
        if (!scene)
        {
            outError = importer.GetErrorString();
            if (outError.empty()) outError = "Assimp failed to read file.";
            return false;
        }

        if (!scene->HasMeshes())
        {
            outError = "Imported scene contains no meshes.";
            return false;
        }

        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            if (!mesh || !mesh->HasPositions()) continue;

            const std::size_t baseVertex = outVertices.size();
            if (baseVertex + mesh->mNumVertices > std::numeric_limits<std::uint16_t>::max())
            {
                outError = "Imported mesh exceeds uint16 index limit.";
                return false;
            }

            outVertices.reserve(outVertices.size() + mesh->mNumVertices);

            for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
            {
                const glm::vec3 position = ToVec3(mesh->mVertices[vertexIndex]);

                Vertex vertex{};
                vertex.Position = position;
                vertex.Color = GetVertexColor(*mesh, vertexIndex, position);

                outVertices.push_back(vertex);
            }

            for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
            {
                const aiFace& face = mesh->mFaces[faceIndex];
                if (face.mNumIndices != 3) continue;

                for (unsigned int index = 0; index < face.mNumIndices; ++index)
                {
                    const std::size_t resolvedIndex = baseVertex + face.mIndices[index];
                    if (resolvedIndex > std::numeric_limits<std::uint16_t>::max())
                    {
                        outError = "Imported mesh index exceeds uint16 range.";
                        return false;
                    }
                    outIndices.push_back(static_cast<std::uint16_t>(resolvedIndex));
                }
            }
        }

        if (outVertices.empty() || outIndices.empty())
        {
            outError = "Imported scene did not produce renderable geometry.";
            return false;
        }
        return true;
    }
}
