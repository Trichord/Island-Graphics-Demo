#include "capsule.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "distance_functions.h"
#include "glm/glm.hpp"
#include "world_vectors.h"

Capsule::Capsule(const glm::vec3 &position, float height, float radius)
    : top{position + WorldVectors::worldUp * height / 2.0f}, bottom{position - WorldVectors::worldUp * height / 2.0f},
      height{height}, radius{radius}
{
}

glm::vec3 Capsule::getPosition() const
{
    return (top + bottom) / 2.0f;
}

void Capsule::setPosition(const glm::vec3 &newPosition)
{
    top = newPosition + WorldVectors::worldUp * height / 2.0f;
    bottom = newPosition - WorldVectors::worldUp * height / 2.0f;
}

Capsule::CollisionResolution Capsule::collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C)
{
    constexpr float epsilon{0.0001f};
    using namespace distanceFunction;

    const auto results =
        std::array{pointTriangle(top, A, B, C), pointTriangle(bottom, A, B, C), segmentSegment(top, bottom, A, B),
                   segmentSegment(top, bottom, A, C), segmentSegment(top, bottom, B, C)};

    // Find the closest triangle from all the tests
    auto closestPoint = std::ranges::min_element(
        results, [](const Result &a, const Result &b) { return a.sqrDistance < b.sqrDistance; });

    // Triangle outside the capsule
    if (closestPoint->sqrDistance >= radius * radius)
        return {};

    glm::vec3 normal{closestPoint->closest.first - closestPoint->closest.second};

    if (glm::length(normal) > epsilon)
        normal = glm::normalize(normal);
    else
        normal = glm::vec3(0.0f);

    float collisionDepth{radius - std::sqrt(closestPoint->sqrDistance)};

    return CollisionResolution{collisionDepth, normal};
}

glm::vec3 Capsule::getBottom() const
{
    return bottom + glm::vec3{0.0f, radius, 0.0f};
}

void Capsule::translate(const glm::vec3 &direction)
{
    top += direction;
    bottom += direction;
}

const glm::vec3 &Capsule::getVelocity() const
{
    return velocity;
}

void Capsule::addVelocity(const glm::vec3 &velocity_)
{
    velocity += velocity_;
}

void Capsule::setVelocity(const glm::vec3 &newVelocity)
{
    velocity = newVelocity;
}

void Capsule::slide(const glm::vec3 &normal)
{
    constexpr float epsilon{0.001f};

    float velocityLength{glm::length(velocity)};

    if (velocityLength < 0.00001)
        return;

    glm::vec3 velocityNormalized{velocity / velocityLength};
    glm::vec3 undesiredMotion = normal * glm::dot(velocityNormalized, normal);
    glm::vec3 desiredMotion = velocityNormalized - undesiredMotion;
    velocity = desiredMotion * velocityLength;

    translate(normal + epsilon);
}

float Capsule::getHeight() const
{
    return glm::distance(bottom, top) + 2.0f * radius;
}