#include "capsule.h"

#include <array>

#include "distance_functions.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
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

    auto closestPoint = std::ranges::min_element(
        results, [](const Result &a, const Result &b) { return a.sqrDistance < b.sqrDistance; });

    if (closestPoint->sqrDistance >= radius * radius)
        return {};

    glm::vec3 normal{closestPoint->closest.first - closestPoint->closest.second};

    if (glm::length(normal) > epsilon)
        normal = glm::normalize(normal);
    else
        normal = glm::vec3(0.0f);

    float depth{radius - std::sqrt(closestPoint->sqrDistance)};

    return CollisionResolution{depth, normal};
}