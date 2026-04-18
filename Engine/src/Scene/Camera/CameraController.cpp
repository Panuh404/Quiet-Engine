#include "CameraController.h"

#include <algorithm>
#include <cmath>

#include "Quiet/Input/Input.h"
#include "glm/glm.hpp"
#include "Quiet/Math/Direction.h"


namespace Quiet::Scene
{
    void CameraController::UpdateFromInput(CameraPilotState &state, const Input::Input &input, float deltaTime)
    {
        const auto [deltaX, deltaY] = input.GetMouseDelta();

        state.YawDegrees += static_cast<float>(deltaX) * state.LookSensitivity;
        state.PitchDegrees -= static_cast<float>(deltaY) * state.LookSensitivity;
        state.PitchDegrees = std::clamp(state.PitchDegrees, -89.0f, 89.0f);

        const glm::vec3 forward = Math::GetForwardVector(state.YawDegrees, state.PitchDegrees);
        const glm::vec3 right = Math::GetRightVector(forward);
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
        if (input.IsKeyDown(Input::KeyCode::Space)) state.Position += up * move;
        if (input.IsKeyDown(Input::KeyCode::Q)) state.Position -= up * move;
        if (input.IsKeyDown(Input::KeyCode::E)) state.Position += up * move;
    }
}
