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
    class EditorCamera
    {
    public:
        EditorCamera();
        ~EditorCamera();

        void Update(const Quiet::Input::Input& input, float deltaTime);
        [[nodiscard]] Quiet::Renderer::Vulkan::CameraUniformData BuildUniformData(float aspect) const;

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