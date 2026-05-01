#include "aabb.h"

#include <array>

#include "glm/glm.hpp"

AABB::AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
    : center_{0.5f * (glm::vec3{minX, minY, minZ} + glm::vec3{maxX, maxY, maxZ})},
      extent_{0.5f * (glm::vec3{maxX, maxY, maxZ} - glm::vec3{minX, minY, minZ})}
{
}

AABB::AABB(const glm::vec3 &min_, const glm::vec3 &max_) : center_{0.5f * (min_ + max_)}, extent_{0.5f * (max_ - min_)}
{
}

AABB AABB::transform(const glm::mat4 &modelMatrix) const
{
    // Take the abs of the modelMatrix so the AABB can never collapse. It will only ever expand to a
    // conservative estimate on rotations.
    glm::mat3 RS{glm::mat3{modelMatrix}};
    glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

    glm::vec3 worldCenter{modelMatrix * glm::vec4(center_, 1.0f)};
    glm::vec3 worldExtent{absRS * extent_};

    return {worldCenter - worldExtent, worldCenter + worldExtent};
}

bool AABB::intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const
{
    const AABB worldAABB{transform(modelMatrix)};

    for (const glm::vec4 &plane : planes)
    {
        float d{glm::dot(worldAABB.center_, glm::vec3{plane})};
        float r{glm::dot(worldAABB.extent_, glm::vec3{glm::abs(plane)})};

        if (d + r < -plane.w)
            return false;
    }

    return true;
}

bool AABB::intersects(const std::array<glm::vec4, 6> &planes) const
{
    for (const glm::vec4 &plane : planes)
    {
        float d{glm::dot(center_, glm::vec3{plane})};
        float r{glm::dot(extent_, glm::vec3{glm::abs(plane)})};

        if (d + r < -plane.w)
            return false;
    }

    return true;
}

void AABB::resize(float scale)
{
    extent_ *= scale;
}

glm::vec3 AABB::getCenter() const
{
    return center_;
}

glm::vec3 AABB::getMin() const
{
    return center_ - extent_;
}

glm::vec3 AABB::getMax() const
{
    return center_ + extent_;
}