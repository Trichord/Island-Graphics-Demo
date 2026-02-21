#pragma once

#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"

class SpatialGrid
{
  public:
    SpatialGrid() = default;
    SpatialGrid(const AABB &worldBounds, float cellSize, const std::vector<glm::vec3> &meshVertices);

    void query(const glm::vec3 &position, std::vector<int> &result);

  private:
    int gridWidth{0};
    float originX{0.0f};
    float originZ{0.0f};
    float cellSize{0.0f};
    std::vector<int> offsets;
    std::vector<int> data;
};