#include "FreeCamera.h"

#include "glm/gtc/matrix_transform.hpp"

#include "CameraController.h"
#include "CameraPilotState.h"
#include "Quiet/Input/Input.h"

namespace Quiet::Scene
{
    FreeCamera::FreeCamera() = default;
    FreeCamera::~FreeCamera() = default;

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
        Renderer::Vulkan::CameraUniformData camera{};
        const glm::vec3 forward = GetForwardVector();

        camera.View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        camera.Projection = glm::perspective(glm::radians(m_FovDegrees), aspect, 0.1f, 100.f);

        if (flipY)
        {
            camera.Projection[1][1] *= -1.0f;
        }
        return camera;
    }

    glm::vec3 FreeCamera::GetForwardVector() const
    {
        const float yawRad = glm::radians(m_Yaw);
        const float pitchRad = glm::radians(m_Pitch);

        glm::vec3 forward;
        forward.x = std::cos(yawRad) * std::cos(pitchRad);
        forward.y = std::sin(pitchRad);
        forward.z = std::sin(yawRad) * std::cos(pitchRad);

        return glm::normalize(forward);
    }

    glm::vec3 FreeCamera::GetRightVector() const
    {
        return glm::normalize(glm::cross(GetForwardVector(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }
}
