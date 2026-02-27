#define GLM_ENABLE_EXPERIMENTAL

#include "capsule.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "distance_functions.h"
#include "glm/glm.hpp"
#include "glm/gtx/closest_point.hpp"
#include "world_vectors.h"

constexpr float epsilon{0.001f};

Capsule::Capsule(const glm::vec3 &position, float height, float radius)
    : top_{position + WorldVectors::worldUp * height / 2.0f}, bottom_{position - WorldVectors::worldUp * height / 2.0f},
      height_{height}, radius_{radius}
{
}

glm::vec3 Capsule::getPosition() const
{
    return (top_ + bottom_) / 2.0f;
}

void Capsule::setPosition(const glm::vec3 &newPosition)
{
    top_ = newPosition + WorldVectors::worldUp * height_ / 2.0f;
    bottom_ = newPosition - WorldVectors::worldUp * height_ / 2.0f;
}

Capsule::CollisionResolution Capsule::collideWithTriangle(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C)
{
    using namespace distanceFunction;

    const auto results =
        std::array{pointTriangle(top_, A, B, C), pointTriangle(bottom_, A, B, C), segmentSegment(top_, bottom_, A, B),
                   segmentSegment(top_, bottom_, A, C), segmentSegment(top_, bottom_, B, C)};

    // Find the closest triangle from all the tests
    auto closestPoint = std::ranges::min_element(
        results, [](const Result &a, const Result &b) { return a.sqrDistance < b.sqrDistance; });

    // Triangle outside the capsule
    if (closestPoint->sqrDistance >= radius_ * radius_)
        return {};

    glm::vec3 normal{closestPoint->closest.first - closestPoint->closest.second};

    if (glm::length(normal) > epsilon)
        normal = glm::normalize(normal);
    else
        normal = glm::vec3(0.0f);

    float collisionDepth{radius_ - std::sqrt(closestPoint->sqrDistance)};

    return CollisionResolution{collisionDepth, normal};
}

glm::vec3 Capsule::getBottom() const
{
    return bottom_ + glm::vec3{0.0f, radius_, 0.0f};
}

void Capsule::translate(const glm::vec3 &direction)
{
    top_ += direction;
    bottom_ += direction;
}

const glm::vec3 &Capsule::getVelocity() const
{
    return velocity_;
}

void Capsule::addVelocity(const glm::vec3 &velocity)
{
    velocity_ += velocity;
}

void Capsule::setVelocity(const glm::vec3 &velocity)
{
    velocity_ = velocity;
}

void Capsule::slide(const glm::vec3 &normal)
{

    float velocityLength{glm::length(velocity_)};

    if (velocityLength < 0.00001)
        return;

    glm::vec3 velocityNormalized{velocity_ / velocityLength};
    glm::vec3 undesiredMotion = normal * glm::dot(velocityNormalized, normal);
    glm::vec3 desiredMotion = velocityNormalized - undesiredMotion;
    velocity_ = desiredMotion * velocityLength;

    translate(normal + epsilon);
}

float Capsule::getHeight() const
{
    return glm::distance(bottom_, top_) + 2.0f * radius_;
}

std::optional<Capsule::CollisionResolution> Capsule::collideWithCircle(const glm::vec3 &center, float otherRadius)
{
    glm::vec3 closestPoint{glm::closestPointOnLine(center, top_, bottom_)};
    float distance{glm::distance(closestPoint, center)};

    if (distance > radius_ + otherRadius)
        return {};

    float depth{(radius_ + otherRadius) - distance};
    glm::vec3 normal{glm::normalize(center - closestPoint)};

    return Capsule::CollisionResolution{depth, normal};
}

std::optional<Capsule::CollisionResolution> Capsule::collideWithAABB(const AABB &aabb)
{
    glm::vec3 closestPoint{glm::closestPointOnLine(aabb.getCenter(), top_, bottom_)};

    // Sphere - AABB intersection by Jim Arvo
    glm::vec3 bMin{aabb.getMin()};
    glm::vec3 bMax{aabb.getMax()};

    float dMin{0.0f};
    float r2{radius_ * radius_};
    for (int i{0}; i < 3; ++i)
    {
        if (closestPoint[i] < bMin[i])
            dMin += (closestPoint[i] - bMin[i]) * (closestPoint[i] - bMin[i]);
        else if (closestPoint[i] > bMax[i])
            dMin += (closestPoint[i] - bMax[i]) * (closestPoint[i] - bMax[i]);
    }
    if (dMin > r2)
        return {};

    // Find normal and depth
    glm::vec3 clampedToAABB{glm::clamp(closestPoint, bMin, bMax)};

    // Closest point is inside the AABB - can't resolve
    if (glm::length(closestPoint - clampedToAABB) < epsilon)
        return {};

    glm::vec3 normal{glm::normalize(closestPoint - clampedToAABB)};
    float depth{radius_ - glm::sqrt(dMin)};

    return Capsule::CollisionResolution{depth, normal};
}