#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Quiet::Scene
{
    struct Transform
    {
        glm::vec3 Position{0.0f, 0.0f, 0.0f};
        glm::vec3 RotationDegrees{0.0f, 0.0f, 0.0f};
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};

        glm::mat4 ToMatrix() const
        {
            glm::mat4 model(1.0f);

            model = glm::translate(model, Position);
            model = glm::rotate(model, glm::radians(RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, Scale);

            return model;
        }
    };
}