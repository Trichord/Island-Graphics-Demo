#include "aabb.h"

#include <array>

#include "glm/glm.hpp"

AABB::AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
    : center{0.5f * (glm::vec3{minX, minY, minZ} + glm::vec3{maxX, maxY, maxZ})},
      extent{0.5f * (glm::vec3{maxX, maxY, maxZ} - glm::vec3{minX, minY, minZ})}
{
}

AABB::AABB(const glm::vec3 &min_, const glm::vec3 &max_) : center{0.5f * (min_ + max_)}, extent{0.5f * (max_ - min_)}
{
}

bool AABB::intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const
{
    // Take the abs of the modelMatrix so the AABB can never collapse. It will only ever expand to a
    // conservative estimate on rotations.
    glm::mat3 RS{glm::mat3{modelMatrix}};
    glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

    glm::vec3 worldCenter{modelMatrix * glm::vec4(center, 1.0f)};
    glm::vec3 worldExtent{absRS * extent};

    for (const auto &plane : planes)
    {
        float d{glm::dot(worldCenter, glm::vec3{plane})};
        float r{glm::dot(worldExtent, glm::vec3{glm::abs(plane)})};

        if (d + r < -plane.w)
            return false;
    }

    return true;
}

bool AABB::intersectsWorldSpace(const std::array<glm::vec4, 6> &planes) const
{
    for (const auto &plane : planes)
    {
        float d{glm::dot(center, glm::vec3{plane})};
        float r{glm::dot(extent, glm::vec3{glm::abs(plane)})};

        if (d + r < -plane.w)
            return false;
    }

    return true;
}

void AABB::resize(float scale)
{
    extent *= scale;
}

glm::vec3 AABB::getCenter() const
{
    return center;
}

glm::vec3 AABB::getExtent() const
{
    return extent;
}

float AABB::getMinX() const
{
    return (center - extent).x;
}

float AABB::getMinY() const
{
    return (center - extent).y;
}

float AABB::getMinZ() const
{
    return (center - extent).z;
}

float AABB::getMaxX() const
{
    return (center + extent).x;
}

float AABB::getMaxY() const
{
    return (center + extent).y;
}

float AABB::getMaxZ() const
{
    return (center + extent).z;
}

glm::vec3 AABB::getMin() const
{
    return center - extent;
}

glm::vec3 AABB::getMax() const
{
    return center + extent;
}