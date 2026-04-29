#include "MeshImporter.h"

#include <limits>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace
{
    glm::vec3 TransformPoint(const aiMatrix4x4& transform, const aiVector3D& point)
    {
        return {
            transform.a1 * point.x + transform.a2 * point.y + transform.a3 * point.z + transform.a4,
            transform.b1 * point.x + transform.b2 * point.y + transform.b3 * point.z + transform.b4,
            transform.c1 * point.x + transform.c2 * point.y + transform.c3 * point.z + transform.c4
        };
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

    bool AppendMesh(const aiMesh& mesh,
                    const aiMatrix4x4& transform,
                    std::vector<Quiet::Renderer::Vulkan::Vertex>& outVertices,
                    std::vector<std::uint16_t>& outIndices,
                    std::string& outError)
    {
        if (!mesh.HasPositions()) return true;

        const std::size_t baseVertex = outVertices.size();
        if (baseVertex + mesh.mNumVertices > std::numeric_limits<std::uint16_t>::max())
        {
            outError = "Imported mesh exceeds uint16 index limit.";
            return false;
        }
        outVertices.reserve(outVertices.size() + mesh.mNumVertices);

        for (unsigned int vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex)
        {
            const glm::vec3 transformedPosition = TransformPoint(transform, mesh.mVertices[vertexIndex]);
            Quiet::Renderer::Vulkan::Vertex vertex{};
            vertex.Position = transformedPosition;
            vertex.Color = GetVertexColor(mesh, vertexIndex, transformedPosition);

            outVertices.push_back(vertex);
        }

        for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
        {
            const aiFace& face = mesh.mFaces[faceIndex];

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

        return true;
    }

    bool TraverseNode(const aiScene& scene,
                      const aiNode& node,
                      const aiMatrix4x4& parentTransform,
                      std::vector<Quiet::Renderer::Vulkan::Vertex>& outVertices,
                      std::vector<std::uint16_t>& outIndices,
                      std::string& outError,
                      std::uint32_t& outImportedMeshCount)
    {
        const aiMatrix4x4 nodeTransform = parentTransform * node.mTransformation;

        for (unsigned int meshIndex = 0; meshIndex < node.mNumMeshes; ++meshIndex)
        {
            const unsigned int sceneMeshIndex = node.mMeshes[meshIndex];
            if (sceneMeshIndex >= scene.mNumMeshes)
            {
                outError = "Node references an invalid mesh index.";
                return false;
            }
            const aiMesh* mesh = scene.mMeshes[sceneMeshIndex];
            if (!mesh) continue;
            if (!AppendMesh(*mesh, nodeTransform, outVertices, outIndices, outError)) return false;

            ++outImportedMeshCount;
        }

        for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
        {
            const aiNode* child = node.mChildren[childIndex];
            if (!child) continue;
            if (!TraverseNode(scene, *child, nodeTransform, outVertices, outIndices, outError, outImportedMeshCount))
                return false;
        }
        return true;
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
            if (outError.empty())
            {
                outError = "Assimp failed to read file.";
            }
            return false;
        }
        if (!scene->mRootNode)
        {
            outError = "Imported scene has no root node.";
            return false;
        }
        if (!scene->HasMeshes())
        {
            outError = "Imported scene contains no meshes.";
            return false;
        }

        std::uint32_t importedMeshCount = 0;
        const aiMatrix4x4 identity{};

        if (!TraverseNode(*scene, *scene->mRootNode, identity, outVertices, outIndices, outError, importedMeshCount))
            return false;

        if (importedMeshCount == 0 || outVertices.empty() || outIndices.empty())
        {
            outError = "Imported scene did not produce renderable geometry.";
            return false;
        }
        return true;
    }
}
