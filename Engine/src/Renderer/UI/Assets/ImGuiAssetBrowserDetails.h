#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <glm/vec3.hpp>

#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace Quiet::Renderer::UI::Assets
{
    void DrawDetailRow(const char* label, const char* value, float labelWidth);
    void DrawDetailRowWrapped(const char* label, const std::string& value, float labelWidth);
    void DrawDetailRowUInt(const char* label, std::uint32_t value, float labelWidth);
    void DrawDetailRowVec3(const char* label, const glm::vec3& value, float labelWidth);

    void DrawAssetDetailsInline(
        const Project::ProjectContext& projectContext,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        const Scene::Scene& scene,
        const std::string& selectedAssetID,
        float uiScale
    );

    void DrawAssetDetailsPopup(
        const Project::ProjectContext& projectContext,
        const Vulkan::MeshLibrary& meshLibrary,
        const Vulkan::TextureLibrary& textureLibrary,
        const std::string& selectedAssetID,
        float uiScale
    );

    void DrawAssetSelectionSummaryInline(
        const Project::ProjectContext& projectContext,
        const std::vector<std::string>& selectedAssetIDs,
        const std::string& primaryAssetID
    );
}