#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Renderer/Vulkan/RenderTypes.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    struct CameraPilotState;

    class FreeCamera
    {
    public:
        FreeCamera() = default;
        ~FreeCamera() = default;

        void Update(const Input::Input& input, float deltaTime);

        [[nodiscard]] Renderer::Vulkan::CameraUniformData BuildUniformData(float aspect, bool flipY) const;

        [[nodiscard]] const glm::vec3& GetPosition() const { return m_Position; }
        [[nodiscard]] float GetMoveSpeed() const { return m_MoveSpeed; }
        [[nodiscard]] float GetLookSensitivity() const { return m_LookSensitivity; }
        [[nodiscard]] float GetFovDegrees() const { return m_FovDegrees; }

        [[nodiscard]] glm::vec3 GetForwardVector() const;
        [[nodiscard]] glm::vec3 GetRightVector() const;

        [[nodiscard]] CameraPilotState BuildPilotState() const;
        void ApplyPilotState(const CameraPilotState& state);

        void SetPosition(const glm::vec3& position) { m_Position = position; }
        void SetMoveSpeed(float moveSpeed);
        void SetPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);
        void FocusOnTarget(const glm::vec3& target, float distance);

    private:
        glm::vec3 m_Position{ 0.0f, 0.0f, 2.5f };
        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;
        float m_MoveSpeed = 2.5f;
        float m_LookSensitivity = 0.08f;
        float m_FovDegrees = 60.0f;
    };
}