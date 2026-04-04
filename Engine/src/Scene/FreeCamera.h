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
    class FreeCamera
    {
    public:
        FreeCamera();
        ~FreeCamera();

        void Update(const Quiet::Input::Input& input, float deltaTime);
        [[nodiscard]] Quiet::Renderer::Vulkan::CameraUniformData BuildUniformData(float aspect, bool flipY) const;

        [[nodiscard]] glm::vec3 GetPosition() const { return m_Position; }
        [[nodiscard]] float GetYaw() const { return m_Yaw; }
        [[nodiscard]] float GetPitch() const { return m_Pitch; }

    private:
        [[nodiscard]] glm::vec3 GetForward() const;
        [[nodiscard]] glm::vec3 GetRight() const;

    private:
        glm::vec3 m_Position{ 0.0f, 0.0f, 2.5f };
        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;
        float m_MoveSpeed = 2.5f;
        float m_LookSensitivity = 0.08;

        bool m_FirstMouseSample = true;
        double m_LastMouseX = 0.0;
        double m_LastMouseY = 0.0;
    };
}