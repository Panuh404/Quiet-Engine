#pragma once

#include <imgui.h>
#include "ImGuizmo.h"

#include <glm/glm.hpp>

namespace Quiet::Scene
{
    struct Transform;
}

namespace Quiet::Editor
{
    enum class GizmoOperation;
    enum class GizmoMode;
    struct EditorGizmoState;

    struct GizmoTransformComponents
    {
        float Translation[3]{};
        float Rotation[3]{};
        float Scale[3]{};
    };

    struct GizmoSnapSettings
    {
        bool Enabled = false;
        float Values[3]{};

        [[nodiscard]] const float* GetPointer() const
        {
            return Enabled ? Values : nullptr;
        }
    };

    [[nodiscard]] ImGuizmo::OPERATION ToImGuizmoOperation(GizmoOperation operation);
    [[nodiscard]] ImGuizmo::MODE ToImGuizmoMode(GizmoMode mode);

    [[nodiscard]] GizmoTransformComponents MakeGizmoTransformComponents(const Scene::Transform& transform);
    [[nodiscard]] glm::mat4 ComposeGizmoMatrix(const GizmoTransformComponents& components);

    void ApplyGizmoMatrixToTransform(const glm::mat4& matrix, Scene::Transform& transform);
    [[nodiscard]] GizmoSnapSettings BuildGizmoSnapSettings(const EditorGizmoState& gizmoState);
}
