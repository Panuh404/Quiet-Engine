#include "Renderer/Vulkan/Mesh/MeshImporter.h"

#include <limits>
#include <string>
#include <algorithm>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/geometric.hpp>

#include "Renderer/Vulkan/Mesh/StaticMesh.h"

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

    glm::vec3 TransformDirection(const aiMatrix4x4& transform, const aiVector3D& direction)
    {
        glm::vec3 transformed{
            transform.a1 * direction.x + transform.a2 * direction.y + transform.a3 * direction.z,
            transform.b1 * direction.x + transform.b2 * direction.y + transform.b3 * direction.z,
            transform.c1 * direction.x + transform.c2 * direction.y + transform.c3 * direction.z
        };

        const float length = glm::length(transformed);
        if (length <= 0.00001f)
        {
            return { 0.0f, 1.0f, 0.0f };
        }

        return transformed / length;
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

    glm::vec2 GetTexCoord(const aiMesh& mesh, const unsigned int vertexIndex)
    {
        if (mesh.HasTextureCoords(0) && mesh.mTextureCoords[0])
        {
            const aiVector3D& texCoord = mesh.mTextureCoords[0][vertexIndex];
            return { texCoord.x, texCoord.y };
        }

        return { 0.0f, 0.0f };
    }

    void IncludePointInBounds(Quiet::Renderer::Vulkan::MeshBounds& bounds,
                              const glm::vec3& point)
    {
        if (!bounds.Valid)
        {
            bounds.Min = point;
            bounds.Max = point;
            bounds.Valid = true;
            return;
        }

        bounds.Min.x = std::min(bounds.Min.x, point.x);
        bounds.Min.y = std::min(bounds.Min.y, point.y);
        bounds.Min.z = std::min(bounds.Min.z, point.z);

        bounds.Max.x = std::max(bounds.Max.x, point.x);
        bounds.Max.y = std::max(bounds.Max.y, point.y);
        bounds.Max.z = std::max(bounds.Max.z, point.z);
    }

    bool AppendMesh(const aiMesh& mesh,
                    const aiMatrix4x4& transform,
                    Quiet::Renderer::Vulkan::MeshImportResult& result)
    {
        if (!mesh.HasPositions()) return true;

        const std::size_t baseVertex = result.Vertices.size();
        const std::size_t baseIndex = result.Indices.size();
        if (baseVertex + mesh.mNumVertices > std::numeric_limits<Quiet::Renderer::Vulkan::Index>::max())
        {
            result.Error = "Imported mesh exceeds uint32 index limit.";
            return false;
        }
        result.Vertices.reserve(result.Vertices.size() + mesh.mNumVertices);

        for (unsigned int vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex)
        {
            const glm::vec3 transformedPosition = TransformPoint(transform, mesh.mVertices[vertexIndex]);
            Quiet::Renderer::Vulkan::Vertex vertex{};
            vertex.Position = transformedPosition;
            vertex.Color = GetVertexColor(mesh, vertexIndex, transformedPosition);

            if (mesh.HasNormals() && mesh.mNormals)
                vertex.Normal = TransformDirection(transform, mesh.mNormals[vertexIndex]);
            else
                vertex.Normal = { 0.0f, 1.0f, 0.0f };

            vertex.TexCoord = GetTexCoord(mesh, vertexIndex);

            result.Vertices.push_back(vertex);
            IncludePointInBounds(result.Bounds, vertex.Position);
        }

        for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
        {
            const aiFace& face = mesh.mFaces[faceIndex];

            if (face.mNumIndices != 3) continue;
            for (unsigned int index = 0; index < face.mNumIndices; ++index)
            {
                const std::size_t resolvedIndex = baseVertex + face.mIndices[index];
                if (resolvedIndex > std::numeric_limits<Quiet::Renderer::Vulkan::Index>::max())
                {
                    result.Error = "Imported mesh index exceeds uint32 range.";
                    return false;
                }
                result.Indices.push_back(static_cast<Quiet::Renderer::Vulkan::Index>(resolvedIndex));
            }
        }

        const std::size_t vertexCount = result.Vertices.size() - baseVertex;
        const std::size_t indexCount = result.Indices.size() - baseIndex;

        Quiet::Renderer::Vulkan::ImportedSubmesh submesh{};
        submesh.Name = mesh.mName.C_Str();
        submesh.BaseVertex = static_cast<std::uint32_t>(baseVertex);
        submesh.VertexCount = static_cast<std::uint32_t>(vertexCount);
        submesh.BaseIndex = static_cast<std::uint32_t>(baseIndex);
        submesh.IndexCount = static_cast<std::uint32_t>(indexCount);
        submesh.MaterialIndex = mesh.mMaterialIndex;

        if (submesh.Name.empty())
        {
            submesh.Name = "Submesh_" + std::to_string(result.Submeshes.size());
        }

        result.Submeshes.push_back(std::move(submesh));

        return true;
    }

    bool TraverseNode(const aiScene& scene,
                      const aiNode& node,
                      const aiMatrix4x4& parentTransform,
                      Quiet::Renderer::Vulkan::MeshImportResult& result)
    {
        const aiMatrix4x4 nodeTransform = parentTransform * node.mTransformation;

        for (unsigned int meshIndex = 0; meshIndex < node.mNumMeshes; ++meshIndex)
        {
            const unsigned int sceneMeshIndex = node.mMeshes[meshIndex];
            if (sceneMeshIndex >= scene.mNumMeshes)
            {
                result.Error = "Node references an invalid mesh index.";
                return false;
            }
            const aiMesh* mesh = scene.mMeshes[sceneMeshIndex];
            if (!mesh) continue;
            if (!AppendMesh(*mesh, nodeTransform, result)) return false;

            ++result.ImportedMeshCount;
        }

        for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
        {
            const aiNode* child = node.mChildren[childIndex];
            if (!child) continue;
            if (!TraverseNode(scene, *child, nodeTransform, result))
                return false;
        }
        return true;
    }
}

namespace Quiet::Renderer::Vulkan
{
    MeshImportResult MeshImporter::LoadFromFile(const std::filesystem::path &path)
    {
        MeshImportResult result{};
        result.ImporterName = "Assimp";

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
            result.Error = importer.GetErrorString();
            if (result.Error.empty())
            {
                result.Error = "Assimp failed to read file.";
            }
            return result;
        }

        if (!scene->mRootNode)
        {
            result.Error = "Imported scene has no root node.";
            return result;
        }

        if (!scene->HasMeshes())
        {
            result.Error = "Imported scene contains no meshes.";
            return result;
        }

        const aiMatrix4x4 identity{};

        if (!TraverseNode(*scene, *scene->mRootNode, identity, result))
        {
            return result;
        }

        if (result.ImportedMeshCount == 0 || !result.HasGeometry())
        {
            result.Error = "Imported scene did not produce renderable geometry.";
            return result;
        }

        return result;
    }
}
