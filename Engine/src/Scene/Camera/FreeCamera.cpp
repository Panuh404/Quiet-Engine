#include "Scene/Camera/FreeCamera.h"

#include <algorithm>
#include "glm/gtc/matrix_transform.hpp"

#include "Scene/Camera/CameraController.h"
#include "Scene/Camera/CameraPilotState.h"
#include "Quiet/Math/Direction.h"


namespace Quiet::Scene
{
    void FreeCamera::Update(const Input::Input& input, float deltaTime)
    {
        CameraPilotState state = BuildPilotState();
        CameraController::UpdateFromInput(state, input, deltaTime);
        ApplyPilotState(state);
    }

    Renderer::Vulkan::CameraUniformData FreeCamera::BuildUniformData(float aspect, bool flipY) const
    {
        Renderer::Vulkan::CameraUniformData data{};

        const glm::vec3 forward = Math::GetForwardVector(m_Yaw, m_Pitch);
        data.View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        data.Projection = glm::perspective(
            glm::radians(m_FovDegrees),
            aspect,
            0.1f,
            1000.0f
        );

        if (flipY) data.Projection[1][1] *= -1.0f;
        return data;
    }

    glm::vec3 FreeCamera::GetForwardVector() const
    {
        return Math::GetForwardVector(m_Yaw, m_Pitch);
    }

    glm::vec3 FreeCamera::GetRightVector() const
    {
        return Math::GetRightVector(GetForwardVector());
    }

    CameraPilotState FreeCamera::BuildPilotState() const
    {
        CameraPilotState state{};
        state.Position = m_Position;
        state.YawDegrees = m_Yaw;
        state.PitchDegrees = m_Pitch;
        state.MoveSpeed = m_MoveSpeed;
        state.LookSensitivity = m_LookSensitivity;
        return state;
    }

    void FreeCamera::ApplyPilotState(const CameraPilotState &state)
    {
        m_Position = state.Position;
        m_Yaw = state.YawDegrees;
        m_Pitch = state.PitchDegrees;
    }

    void FreeCamera::SetMoveSpeed(float moveSpeed)
    {
        m_MoveSpeed = std::clamp(moveSpeed, 0.1f, 100.0f);
    }

    void FreeCamera::SetPose(const glm::vec3 &position, float yawDegrees, float pitchDegrees)
    {
        m_Position = position;
        m_Yaw = yawDegrees;
        m_Pitch = pitchDegrees;
    }

    void FreeCamera::FocusOnTarget(const glm::vec3 &target, float distance)
    {
        const glm::vec3 forward = Math::GetForwardVector(m_Yaw, m_Pitch);
        m_Position = target - forward * distance;
    }
}
