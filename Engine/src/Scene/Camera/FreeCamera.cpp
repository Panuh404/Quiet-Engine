#include "FreeCamera.h"

#include "glm/gtc/matrix_transform.hpp"

#include "CameraController.h"
#include "CameraPilotState.h"
#include "Quiet/Input/Input.h"

namespace
{
    glm::vec3 GetForward(float yawDegrees, float pitchDegrees)
    {
        const float yaw = glm::radians(yawDegrees);
        const float pitch = glm::radians(pitchDegrees);

        glm::vec3 forward{};
        forward.x = std::cos(yaw) * std::cos(pitch);
        forward.y = std::sin(pitch);
        forward.z = std::sin(yaw) * std::cos(pitch);

        return glm::normalize(forward);
    }

    glm::vec3 GetRight(const glm::vec3& forward)
    {
        return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    }
}

namespace Quiet::Scene
{
    void FreeCamera::Update(const Input::Input& input, float deltaTime)
    {
        CameraPilotState state{};
        state.Position = m_Position;
        state.YawDegrees = m_Yaw;
        state.PitchDegrees = m_Pitch;
        state.MoveSpeed = m_MoveSpeed;
        state.LookSensitivity = m_LookSensitivity;

        CameraController::UpdateFromInput(state, input, deltaTime);

        m_Position = state.Position;
        m_Yaw = state.YawDegrees;
        m_Pitch = state.PitchDegrees;
    }

    Renderer::Vulkan::CameraUniformData FreeCamera::BuildUniformData(float aspect, bool flipY) const
    {
        Renderer::Vulkan::CameraUniformData data{};

        const glm::vec3 forward = GetForward(m_Yaw, m_Pitch);
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
        return GetForward(m_Yaw, m_Pitch);
    }

    glm::vec3 FreeCamera::GetRightVector() const
    {
        return GetRight(GetForwardVector());
    }

    void FreeCamera::SetPose(const glm::vec3 &position, float yawDegrees, float pitchDegrees)
    {
        m_Position = position;
        m_Yaw = yawDegrees;
        m_Pitch = pitchDegrees;
    }

    void FreeCamera::FocusOnTarget(const glm::vec3 &target, float distance)
    {
        const glm::vec3 forward = GetForward(m_Yaw, m_Pitch);
        m_Position = target - forward * distance;
    }
}
