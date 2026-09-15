#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>

#include "Quiet/Core/UUID.h"

namespace Quiet::Editor
{
    class EditorCommands;
}

namespace Quiet::Renderer::Vulkan
{
    class TextureLibrary;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::UI::Assets
{
    struct AssetPickerOptions;
}

namespace Quiet::Scene
{
    class Scene;
    struct MaterialComponent;
    class Actor;
    enum class RigidBodyMotionType : std::uint8_t;
}

namespace Quiet::Renderer::UI::Inspector
{
    struct TrackedInspectorInteraction;
    struct InspectorState;
    struct ComponentHeaderResult
    {
        bool Open = false;
        bool RemoveRequested = false;
        bool ResetRequested = false;
        bool CopyRequested = false;
        bool PasteRequested = false;
    };

    void DrawInspectorHint(const char* text);
    void DrawInspectorWarning(const char* text);
    void DrawInspectorSubsectionHeader(const std::string& label);
    void BeginInspectorPropertyRow(const char* label);
    const char* GetRigidBodyMotionTypeLabel(const Scene::RigidBodyMotionType type);

    void DrawInspectorBanner(const char* message);
    void DrawInspectorEmptyState();

    void DrawFloatingComponentOptionsMenu(InspectorState& state,
                                          const char* popupID,
                                          const bool removable,
                                          const bool resettable,
                                          const bool clipboardSupported,
                                          const bool pasteAvailable,
                                          const char* pasteUnavailableReason,
                                          ComponentHeaderResult& result);

    ComponentHeaderResult BeginInspectorComponent(InspectorState& state,
                                                  const char* title,
                                                  const char* popupID,
                                                  bool removable = true,
                                                  bool resettable = false,
                                                  bool clipboardSupported = false,
                                                  bool pasteAvailable = false,
                                                  const char* pasteUnavailableReason = nullptr);

    bool DrawCheckboxPropertyRow(const char* label, bool& value);
    bool DrawRigidBodyMotionTypePropertyRow(const char* label, Scene::RigidBodyMotionType& value);
    bool DrawAssetPickerPropertyRow(const char* label,
                                    const Project::ProjectContext& projectContext,
                                    const Assets::AssetPickerOptions& options,
                                    const std::string& currentAssetID,
                                    std::string& outSelectedAssetID);

    bool DrawIntPropertyRow(const char* label, std::int64_t& value, const TrackedInspectorInteraction& tracked);
    bool DrawStringPropertyRow(const char* label, std::string& value, const TrackedInspectorInteraction& tracked);
    bool DrawFloatPropertyRow(const char* label, float& value, const float speed, const float minValue, const float maxValue, const TrackedInspectorInteraction& tracked);
    bool DrawVec2PropertyRow(const char* label, glm::vec2& value, float speed, float minValue, float maxValue, const TrackedInspectorInteraction& tracked);
    bool DrawVec3PropertyRow(const char* label, glm::vec3& value, const float speed, const float minValue, const float maxValue, const TrackedInspectorInteraction& tracked);
    bool DrawVec4PropertyRow(const char* label, glm::vec4& value, float speed, float minValue, float maxValue, const TrackedInspectorInteraction& tracked);
    bool DrawColor3PropertyRow(const char* label, glm::vec3& value, const TrackedInspectorInteraction& tracked);
    bool DrawColor4PropertyRow(const char* label, glm::vec4& value, const TrackedInspectorInteraction& tracked);
    bool DrawActorPickerPropertyRow(const char* label, const Scene::Scene& scene, Core::UUID currentActorID, Core::UUID& outSelectedActorID);

    std::string GetMaterialSlotSourceName(const Scene::Actor& actor, const std::uint32_t slotIndex);
    std::size_t CountSubmeshesUsingMaterialSlot(const Scene::Actor& actor, const std::uint32_t slotIndex);
    std::string MakeMaterialSlotUsageLabel(const std::size_t submeshCount);
    void DrawCompactMaterialTexturePreview(const std::string& textureAssetID, const Vulkan::TextureLibrary& textureLibrary);
    bool DrawMaterialSlotHeader(const Scene::Actor& actor, const Scene::MaterialComponent& materialSlot, const std::uint32_t slotIndex, const bool defaultOpen);
    void DrawMaterialSlotEditor(Scene::Scene& scene,
                                Scene::Actor& actor,
                                Editor::EditorCommands& commands,
                                const Project::ProjectContext& projectContext,
                                Scene::MaterialComponent& materialSlot,
                                std::uint32_t slotIndex,
                                const Vulkan::TextureLibrary& textureLibrary,
                                const TrackedInspectorInteraction& tracked,
                                bool hasTextureAssets,
                                bool defaultOpen);
}