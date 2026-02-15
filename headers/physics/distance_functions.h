#pragma once

#include "glm/glm.hpp"
#include <utility>

namespace distanceFunction
{

struct Result
{
    float distance{0.0f};
    float sqrDistance{0.0f};
    std::pair<glm::vec3, glm::vec3> closest{glm::vec3{0.0f}, glm::vec3{0.0f}};
};

Result pointTriangle(const glm::vec3 &point, const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);
Result segmentSegment(const glm::vec3 &P0, const glm::vec3 &P1, const glm::vec3 Q0, const glm::vec3 &Q1);

} // namespace distanceFunction