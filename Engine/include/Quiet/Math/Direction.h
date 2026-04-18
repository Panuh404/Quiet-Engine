#pragma once

#include <cmath>

#include "glm/glm.hpp"

namespace Quiet::Math
{
    inline glm::vec3 GetForwardVector(float yawDegrees, float pitchDegrees)
    {
        const float yaw = glm::radians(yawDegrees);
        const float pitch = glm::radians(pitchDegrees);

        glm::vec3 forward{};
        forward.x = std::cos(yaw) * std::cos(pitch);
        forward.y = std::sin(pitch);
        forward.z = std::sin(yaw) * std::cos(pitch);

        return glm::normalize(forward);
    }

    inline glm::vec3 GetRightVector(const glm::vec3& forward)
    {
        return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    }
}