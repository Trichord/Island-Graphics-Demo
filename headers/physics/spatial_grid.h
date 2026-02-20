#pragma once

#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"

class SpatialGrid
{
  public:
    SpatialGrid(const AABB &worldBounds, int cellSize, const std::vector<glm::vec3> &meshVertices);

    std::vector<int> query(float worldX, float worldZ);
    std::vector<int> query(const AABB &bounds);

  private:
    int cellSize{0};
    std::size_t gridWidth{0};
    float originX{0.0f};
    float originZ{0.0f};
    std::vector<int> offsets;
    std::vector<int> data;
};