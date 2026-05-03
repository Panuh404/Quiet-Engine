#include "Editor/Viewport/EditorViewportGizmo.h"

#include "glm/gtc/type_ptr.hpp"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Transform.h"

namespace Quiet::Editor
{
    ImGuizmo::OPERATION ToImGuizmoOperation(const GizmoOperation operation)
    {
        switch (operation)
        {
            case GizmoOperation::Translate: return ImGuizmo::TRANSLATE;
            case GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:     return ImGuizmo::SCALE;
        }
        return ImGuizmo::TRANSLATE;
    }

    ImGuizmo::MODE ToImGuizmoMode(const GizmoMode mode)
    {
        switch (mode)
        {
            case GizmoMode::Local:  return ImGuizmo::LOCAL;
            case GizmoMode::World:  return ImGuizmo::WORLD;
        }
        return ImGuizmo::WORLD;
    }


    GizmoTransformComponents MakeGizmoTransformComponents(const Scene::Transform& transform)
    {
        GizmoTransformComponents components{};

        components.Translation[0] = transform.Position.x;
        components.Translation[1] = transform.Position.y;
        components.Translation[2] = transform.Position.z;

        components.Rotation[0] = transform.RotationDegrees.x;
        components.Rotation[1] = transform.RotationDegrees.y;
        components.Rotation[2] = transform.RotationDegrees.z;

        components.Scale[0] = transform.Scale.x;
        components.Scale[1] = transform.Scale.y;
        components.Scale[2] = transform.Scale.z;

        return components;
    }

    glm::mat4 ComposeGizmoMatrix(const GizmoTransformComponents& components)
    {
        float modelArray[16]{};

        ImGuizmo::RecomposeMatrixFromComponents(
            components.Translation,
            components.Rotation,
            components.Scale,
            modelArray
        );

        return glm::make_mat4(modelArray);
    }

    void ApplyGizmoMatrixToTransform(const glm::mat4& matrix,
                                     Scene::Transform& transform)
    {
        float outTranslation[3]{};
        float outRotation[3]{};
        float outScale[3]{};

        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(matrix),
            outTranslation,
            outRotation,
            outScale
        );

        transform.Position =
        {
            outTranslation[0],
            outTranslation[1],
            outTranslation[2]
        };

        transform.RotationDegrees =
        {
            outRotation[0],
            outRotation[1],
            outRotation[2]
        };

        transform.Scale =
        {
            outScale[0],
            outScale[1],
            outScale[2]
        };
    }

    GizmoSnapSettings BuildGizmoSnapSettings(const EditorGizmoState& gizmoState)
    {
        GizmoSnapSettings snap{};
        if (!gizmoState.SnapEnabled) return snap;

        snap.Enabled = true;
        switch (gizmoState.Operation)
        {
            case GizmoOperation::Translate:
                snap.Values[0] = gizmoState.TranslateSnap[0];
                snap.Values[1] = gizmoState.TranslateSnap[1];
                snap.Values[2] = gizmoState.TranslateSnap[2];
                break;

            case GizmoOperation::Rotate:
                snap.Values[0] = gizmoState.RotateSnap;
                break;

            case GizmoOperation::Scale:
                snap.Values[0] = gizmoState.ScaleSnap;
                break;
        }

        return snap;
    }
}
