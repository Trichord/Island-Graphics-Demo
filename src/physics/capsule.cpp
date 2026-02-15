#include "capsule.h"
#include "distance_functions.h"
#include "glm/glm.hpp"
#include "world_vectors.h"

Capsule::Capsule(const glm::vec3 &position, float radius, float height)
    : top{position + WorldVectors::worldUp * height / 2.0f}, bottom{position - WorldVectors::worldUp * height / 2.0f},
      radius{radius}
{
}

bool Capsule::collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C)
{
    using namespace distanceFunction;

    Result topWithTriangle{pointTriangle(top, A, B, C)};
    Result bottomWithTriangle{pointTriangle(bottom, A, B, C)};
    Result segWithAB{segmentSegment(top, bottom, A, B)};
    Result segWithAC{segmentSegment(top, bottom, A, C)};
    Result segWithBC{segmentSegment(top, bottom, B, C)};

    float minDistance{std::min({topWithTriangle.sqrDistance, bottomWithTriangle.sqrDistance, segWithAB.sqrDistance,
                                segWithAC.sqrDistance, segWithBC.sqrDistance})};

    return minDistance < radius * radius;
}