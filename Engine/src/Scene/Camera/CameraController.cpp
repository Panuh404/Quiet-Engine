#include "Scene/Camera/CameraController.h"

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

    void CameraController::OrbitAround(CameraPilotState &state, const glm::vec3 &pivot, double mouseDeltaX, double mouseDeltaY)
    {
        state.YawDegrees += static_cast<float>(mouseDeltaX) * state.LookSensitivity;
        state.PitchDegrees -= static_cast<float>(mouseDeltaY) * state.LookSensitivity;
        state.PitchDegrees = std::clamp(state.PitchDegrees, -89.0f, 89.0f);

        const float orbitDistance = std::max(glm::length(state.Position - pivot), 0.1f);
        const glm::vec3 forward = Math::GetForwardVector(state.YawDegrees, state.PitchDegrees);

        state.Position = pivot - forward * orbitDistance;
    }

    void CameraController::Pan(CameraPilotState &state, glm::vec3 &pivot, double mouseDeltaX, double mouseDeltaY, float viewportHeight, float fieldOfViewDegrees)
    {
        if (viewportHeight <= 1.0f)
            return;

        const glm::vec3 forward = Math::GetForwardVector(state.YawDegrees, state.PitchDegrees);
        const glm::vec3 right = Math::GetRightVector(forward);
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        const float pivotDistance = std::max(glm::length(state.Position - pivot), 0.1f);
        const float safeFov = std::clamp(fieldOfViewDegrees, 1.0f, 179.0f);

        const float visibleHeight = 2.0f * pivotDistance * std::tan(glm::radians(safeFov) * 0.5f);
        const float unitsPerPixel = visibleHeight / viewportHeight;

        const glm::vec3 translation =
            (
                -right * static_cast<float>(mouseDeltaX) +
                up * static_cast<float>(mouseDeltaY)
            ) * unitsPerPixel;

        state.Position += translation;
        pivot += translation;
    }
}
