#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"
#include "spatial_grid.h"

// Two-pass construction as described by Christer Ericson
SpatialGrid::SpatialGrid(const AABB &worldBounds, int cellSize, const std::vector<glm::vec3> &meshVertices)
    : cellSize{cellSize}, originX{worldBounds.getMinX()}, originZ{worldBounds.getMinZ()}
{
    if (meshVertices.empty())
        return;

    if (meshVertices.size() % 3 != 0)
        throw std::runtime_error("ERROR: Mesh has incomplete triangles");

    // Force a square grid based of world AABB width
    gridWidth = static_cast<int>(worldBounds.getMaxX() - worldBounds.getMinX());

    std::vector<int> counts(gridWidth * gridWidth, 0);

    // First pass - count how many cells each triangle's AABB overlaps
    for (int i{0}; i < meshVertices.size() - 3; i += 3)
    {
        // Find min max of each triangle
        float minX{std::numeric_limits<float>::infinity()};
        float minZ{std::numeric_limits<float>::infinity()};
        float maxX{-std::numeric_limits<float>::infinity()};
        float maxZ{-std::numeric_limits<float>::infinity()};

        for (int j{0}; j < 3; ++j)
        {
            minX = std::min(minX, meshVertices[i + j].x);
            minZ = std::min(minZ, meshVertices[i + j].z);
            maxX = std::max(maxX, meshVertices[i + j].x);
            maxZ = std::max(maxZ, meshVertices[i + j].z);
        }

        // Shifted by origin
        std::size_t cellMinX{static_cast<size_t>((minX - originX) / cellSize)};
        std::size_t cellMaxX{static_cast<size_t>((maxX - originX) / cellSize)};
        std::size_t cellMinZ{static_cast<size_t>((minZ - originZ) / cellSize)};
        std::size_t cellMaxZ{static_cast<size_t>((maxZ - originZ) / cellSize)};

        cellMinX = std::clamp(cellMinX, std::size_t{0}, gridWidth - 1);
        cellMaxX = std::clamp(cellMaxX, std::size_t{0}, gridWidth - 1);
        cellMinZ = std::clamp(cellMinZ, std::size_t{0}, gridWidth - 1);
        cellMaxZ = std::clamp(cellMaxZ, std::size_t{0}, gridWidth - 1);

        // Count touching cells
        for (std::size_t row{cellMinZ}; row <= cellMaxZ; ++row)
            for (std::size_t col{cellMinX}; col <= cellMaxX; ++col)
                counts[row * gridWidth + col]++;
    }

    // Exclusive prefix sum
    offsets.resize(gridWidth * gridWidth + 1);

    for (int i{1}; i < offsets.size() + 1; ++i)
        offsets[i] = offsets[i - 1] + counts[i - 1];

    data.resize(offsets.back());

    //Second pass
}

std::vector<int> SpatialGrid::query(float worldX, float worldZ)
{
    int cellX = static_cast<int>(worldX - originX) / cellSize;
    int cellZ = static_cast<int>(worldZ - originZ) / cellSize;

    if (cellX < 0 || cellZ < 0 || cellX >= gridWidth || cellZ >= gridWidth)
        return {};
}

std::vector<int> SpatialGrid::query(const AABB &bounds)
{
}