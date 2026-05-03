#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


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

    [[nodiscard]] inline glm::vec3 SafeNormalize(const glm::vec3& value, const glm::vec3& fallback)
    {
        const float length = glm::length(value);
        if (length <= 0.00001f) return fallback;
        return value / length;
    }

    [[nodiscard]] inline glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point)
    {
        return glm::vec3(transform * glm::vec4(point, 1.0f));
    }

    [[nodiscard]] inline std::array<glm::vec3, 8> BuildAABBCorners(const glm::vec3& boundsMin, const glm::vec3& boundsMax)
    {
        return {
            glm::vec3{ boundsMin.x, boundsMin.y, boundsMin.z },
            glm::vec3{ boundsMax.x, boundsMin.y, boundsMin.z },
            glm::vec3{ boundsMin.x, boundsMax.y, boundsMin.z },
            glm::vec3{ boundsMax.x, boundsMax.y, boundsMin.z },
            glm::vec3{ boundsMin.x, boundsMin.y, boundsMax.z },
            glm::vec3{ boundsMax.x, boundsMin.y, boundsMax.z },
            glm::vec3{ boundsMin.x, boundsMax.y, boundsMax.z },
            glm::vec3{ boundsMax.x, boundsMax.y, boundsMax.z }
        };
    }

    [[nodiscard]] inline AABB TransformAABB(const glm::mat4& transform, const glm::vec3& localMin, const glm::vec3& localMax)
    {
        const auto corners = BuildAABBCorners(localMin, localMax);

        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(std::numeric_limits<float>::lowest());

        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 p = TransformPoint(transform, corner);

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
