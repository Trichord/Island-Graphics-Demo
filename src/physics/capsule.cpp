#include "capsule.h"
#include "distance_functions.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world_vectors.h"

Capsule::Capsule(const glm::vec3 &position, float radius, float height)
    : top{position + WorldVectors::worldUp * height / 2.0f}, bottom{position - WorldVectors::worldUp * height / 2.0f},
      radius{radius}, height{height}
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

std::optional<glm::vec3> Capsule::collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C)
{
    using namespace distanceFunction;

    Result topWithTriangle{pointTriangle(top, A, B, C)};
    Result bottomWithTriangle{pointTriangle(bottom, A, B, C)};
    Result segWithAB{segmentSegment(top, bottom, A, B)};
    Result segWithAC{segmentSegment(top, bottom, A, C)};
    Result segWithBC{segmentSegment(top, bottom, B, C)};

    float minDistance{std::min({topWithTriangle.sqrDistance, bottomWithTriangle.sqrDistance, segWithAB.sqrDistance,
                                segWithAC.sqrDistance, segWithBC.sqrDistance})};

    if (minDistance >= radius * radius)
        return {};

    glm::vec3 normal{0.0f};

    // Find out which test succeded
    if (minDistance == topWithTriangle.sqrDistance)
        normal = glm::normalize(topWithTriangle.closest.first - topWithTriangle.closest.second);

    else if (minDistance == bottomWithTriangle.sqrDistance)
        normal = glm::normalize(bottomWithTriangle.closest.first - bottomWithTriangle.closest.second);

    else if (minDistance == segWithAB.sqrDistance)
        normal = glm::normalize(segWithAB.closest.first - segWithAB.closest.second);

    else if (minDistance == segWithAC.sqrDistance)
        normal = glm::normalize(segWithAC.closest.first - segWithAC.closest.second);

    else
        normal = glm::normalize(segWithBC.closest.first - segWithBC.closest.second);

    float penetration{radius - std::sqrt(minDistance)};

    return glm::vec3{normal * penetration};
}