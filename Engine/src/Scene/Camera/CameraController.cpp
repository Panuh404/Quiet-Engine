#include "CameraController.h"

#include <algorithm>
#include <cmath>

#include "Quiet/Input/Input.h"
#include "glm/glm.hpp"


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
    void CameraController::UpdateFromInput(CameraPilotState &state, const Input::Input &input, float deltaTime)
    {
        const auto [deltaX, deltaY] = input.GetMouseDelta();

        state.YawDegrees += static_cast<float>(deltaX) * state.LookSensitivity;
        state.PitchDegrees -= static_cast<float>(deltaY) * state.LookSensitivity;
        state.PitchDegrees = std::clamp(state.PitchDegrees, -89.0f, 89.0f);

        const glm::vec3 forward = GetForward(state.YawDegrees, state.PitchDegrees);
        const glm::vec3 right = GetRight(forward);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);

        float speed = state.MoveSpeed;
        if (input.IsKeyDown(Input::KeyCode::LeftShift) ||
            input.IsKeyDown(Input::KeyCode::RightShift))
        {
            speed *= 2.0f;
        }

        const float move = speed * deltaTime;
        if (input.IsKeyDown(Input::KeyCode::W)) state.Position += forward * move;
        if (input.IsKeyDown(Input::KeyCode::S)) state.Position -= forward * move;
        if (input.IsKeyDown(Input::KeyCode::A)) state.Position -= right * move;
        if (input.IsKeyDown(Input::KeyCode::D)) state.Position += right * move;
        if (input.IsKeyDown(Input::KeyCode::Q)) state.Position -= up * move;
        if (input.IsKeyDown(Input::KeyCode::E)) state.Position += up * move;
    }
}
