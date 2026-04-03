#include "EditorCamera.h"
#include "Quiet/Platform/Window.h"

#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

#include <algorithm>

#include "Input/Input.h"

namespace Quiet::Scene
{
    EditorCamera::EditorCamera() = default;
    EditorCamera::~EditorCamera() = default;

    void EditorCamera::Update(const Quiet::Input::Input& input, float deltaTime)
    {
        auto [deltaX, deltaY] = input.GetMouseDelta();

        m_Yaw += static_cast<float>(deltaX) * m_LookSensitivity;
        m_Pitch -= static_cast<float>(deltaY) * m_LookSensitivity;
        m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

        const glm::vec3 forward = GetForward();
        const glm::vec3 right = GetRight();
        const glm::vec3 up (0.0f,1.0f,0.0f);

        float speed = m_MoveSpeed;
        if (input.IsKeyDown(GLFW_KEY_LEFT_SHIFT)) { speed *= 2.f; }

        const float move = speed * deltaTime;
        if (input.IsKeyDown(GLFW_KEY_W)) m_Position += forward * move;
        if (input.IsKeyDown(GLFW_KEY_S)) m_Position -= forward * move;
        if (input.IsKeyDown(GLFW_KEY_D)) m_Position += right * move;
        if (input.IsKeyDown(GLFW_KEY_A)) m_Position -= right * move;
        if (input.IsKeyDown(GLFW_KEY_SPACE)) m_Position += up * move;
        if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) m_Position -= up * move;
    }

    Quiet::Renderer::Vulkan::CameraUniformData EditorCamera::BuildUniformData(float aspect) const
    {
        Quiet::Renderer::Vulkan::CameraUniformData camera{};
        const glm::vec3 forward = GetForward();

        camera.View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        camera.Projection = glm::perspective(glm::radians(60.f), aspect, 0.1f, 100.f);
        camera.Projection[1][1] *= -1.0f;
        return camera;
    }

    glm::vec3 EditorCamera::GetForward() const
    {
        const float yawRad = glm::radians(m_Yaw);
        const float pitchRad = glm::radians(m_Pitch);

        glm::vec3 forward;
        forward.x = std::cos(yawRad) * std::cos(pitchRad);
        forward.y = std::sin(pitchRad);
        forward.z = std::sin(yawRad) * std::cos(pitchRad);

        return glm::normalize(forward);
    }

    glm::vec3 EditorCamera::GetRight() const
    {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }
}
