#include "models/aabb.h"

#include <array>
#include <glm/glm.hpp>

AABB::AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
    : center_{0.5f * (glm::vec3{minX, minY, minZ} + glm::vec3{maxX, maxY, maxZ})},
      halfExtent_{0.5f * (glm::vec3{maxX, maxY, maxZ} - glm::vec3{minX, minY, minZ})}
{
}

AABB::AABB(const glm::vec3 &min_, const glm::vec3 &max_)
    : center_{0.5f * (min_ + max_)}, halfExtent_{0.5f * (max_ - min_)}
{
}

AABB AABB::transform(const glm::mat4 &modelMatrix) const
{
    // Take the abs of the modelMatrix so the AABB can never collapse. It will only ever expand to a
    // conservative estimate on rotations.
    glm::mat3 RS{glm::mat3{modelMatrix}};
    glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

    glm::vec3 worldCenter{modelMatrix * glm::vec4(center_, 1.0f)};
    glm::vec3 worldExtent{absRS * halfExtent_};

    return {worldCenter - worldExtent, worldCenter + worldExtent};
}

bool AABB::intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const
{
    const AABB worldAABB{transform(modelMatrix)};
    return aabbIntersects(planes, worldAABB.getCenter(), worldAABB.getHalfExtent());
}

bool AABB::intersects(const std::array<glm::vec4, 6> &planes) const
{
    return aabbIntersects(planes, center_, halfExtent_);
}

bool AABB::pointInside(const glm::vec3 &point) const
{
    glm::vec3 d{point - center_};

    // Bitwise and the result for a branchless test
    return (std::abs(d.x) <= halfExtent_.x) & (std::abs(d.y) <= halfExtent_.y) & (std::abs(d.z) <= halfExtent_.z);
}

void AABB::resize(float scale)
{
    halfExtent_ *= scale;
}

const glm::vec3 &AABB::getCenter() const
{
    return center_;
}

const glm::vec3 &AABB::getHalfExtent() const
{
    return halfExtent_;
}

void AABB::setCenter(const glm::vec3 &center)
{
    center_ = center;
}

void AABB::setHalfExtent(const glm::vec3 &extent)
{
    halfExtent_ = extent;
}

glm::vec3 AABB::getMin() const
{
    return center_ - halfExtent_;
}

glm::vec3 AABB::getMax() const
{
    return center_ + halfExtent_;
}

bool AABB::aabbIntersects(const std::array<glm::vec4, 6> &planes, const glm::vec3 &center,
                          const glm::vec3 &halfExtent) const
{
    for (const glm::vec4 &plane : planes)
    {
        float d{glm::dot(center, glm::vec3{plane})};
        float r{glm::dot(halfExtent, glm::vec3{glm::abs(plane)})};

        if (d + r < -plane.w)
            return false;
    }

    return true;
}