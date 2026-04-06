#pragma once

#include "glm/fwd.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace Quiet::Math
{
    struct Ray
    {
        glm::vec3 Origin{ 0.0f };
        glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };
    };

    struct AABB
    {
        glm::vec3 Min{ 0.0f };
        glm::vec3 Max{ 0.0f };
    };

    [[nodiscard]] inline AABB TransformAABB(const glm::mat4& transform, const glm::vec3& localMin, const glm::vec3& localMax)
    {
        const glm::vec3 corners[8] =
        {
            { localMin.x, localMin.y, localMin.z },
            { localMax.x, localMin.y, localMin.z },
            { localMin.x, localMax.y, localMin.z },
            { localMax.x, localMax.y, localMin.z },
            { localMin.x, localMin.y, localMax.z },
            { localMax.x, localMin.y, localMax.z },
            { localMin.x, localMax.y, localMax.z },
            { localMax.x, localMax.y, localMax.z }
        };

        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(std::numeric_limits<float>::lowest());

        for (const glm::vec3& corner : corners)
        {
            const glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
            const glm::vec3 p = glm::vec3(transformed);

            worldMin = glm::min(worldMin, p);
            worldMax = glm::max(worldMax, p);
        }

        return { worldMin, worldMax };
    }

    [[nodiscard]] inline bool IntersectRayAABB(const Ray& ray, const AABB& box, float& hitDistance)
    {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = ray.Origin[axis];
            const float direction = ray.Direction[axis];
            const float minValue = box.Min[axis];
            const float maxValue = box.Max[axis];

            if (std::abs(direction) < 0.000001f)
            {
                if (origin < minValue || origin > maxValue)
                {
                    return false;
                }
                continue;
            }

            const float invDir = 1.0f / direction;
            float t0 = (minValue - origin) * invDir;
            float t1 = (maxValue - origin) * invDir;

            if (t0 > t1)
            {
                std::swap(t0, t1);
            }

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);

            if (tMax < tMin)
            {
                return false;
            }
        }

        hitDistance = tMin;
        return true;
    }
}
