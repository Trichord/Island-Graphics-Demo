#pragma once

#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"

class SpatialGrid
{
  public:
    SpatialGrid() = default;
    SpatialGrid(const AABB &worldBounds, float cellSize, const std::vector<glm::vec3> &terrainVertices);

    void query(const glm::vec3 &position, std::vector<int> &result);

  private:
    struct CellBounds
    {
        int minX;
        int maxX;
        int minZ;
        int maxZ;
    };

    int gridWidth_{0};
    int gridHeight_{0};
    float originX_{0.0f};
    float originZ_{0.0f};
    float cellSize_{0.0f};

    std::vector<int> offsets_;
    std::vector<int> data_;

    CellBounds getMinMaxBounds(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);
    CellBounds getMinMaxBounds(const AABB &aabb);
};