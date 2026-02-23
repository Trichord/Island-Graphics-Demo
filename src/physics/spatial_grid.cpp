#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"
#include "spatial_grid.h"

// Two-pass construction as described by Christer Ericson
SpatialGrid::SpatialGrid(const AABB &worldBounds, float cellSize, const std::vector<glm::vec3> &meshVertices)
    : cellSize{cellSize}, originX{worldBounds.getMinX()}, originZ{worldBounds.getMinZ()}
{
    struct CellBounds
    {
        int minX;
        int maxX;
        int minZ;
        int maxZ;
    };

    // Helper to find minimum and maximum cells in the xz plane touched by each triangle
    auto getMinMax = [&](const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C) {
        float minX = std::min({A.x, B.x, C.x});
        float minZ = std::min({A.z, B.z, C.z});
        float maxX = std::max({A.x, B.x, C.x});
        float maxZ = std::max({A.z, B.z, C.z});

        // Shifted by origin
        int cellMinX{static_cast<int>((minX - originX) / cellSize)};
        int cellMaxX{static_cast<int>((maxX - originX) / cellSize)};
        int cellMinZ{static_cast<int>((minZ - originZ) / cellSize)};
        int cellMaxZ{static_cast<int>((maxZ - originZ) / cellSize)};

        cellMinX = std::clamp(cellMinX, 0, gridWidth - 1);
        cellMaxX = std::clamp(cellMaxX, 0, gridWidth - 1);
        cellMinZ = std::clamp(cellMinZ, 0, gridWidth - 1);
        cellMaxZ = std::clamp(cellMaxZ, 0, gridWidth - 1);

        return CellBounds{cellMinX, cellMaxX, cellMinZ, cellMaxZ};
    };

    if (meshVertices.empty())
        return;

    if (meshVertices.size() % 3 != 0)
        throw std::runtime_error("ERROR: Mesh has incomplete triangles");

    // Force a square grid based of world AABB width
    gridWidth = static_cast<int>(std::ceilf(worldBounds.getMaxX() - worldBounds.getMinX()) / cellSize);

    std::vector<int> counts(gridWidth * gridWidth, 0);

    // First pass - count how many cells each triangle's AABB overlaps
    for (std::size_t i{0}; i < meshVertices.size(); i += 3)
    {
        CellBounds bounds{getMinMax(meshVertices[i], meshVertices[i + 1], meshVertices[i + 2])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};

        // Count touching cells
        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
                counts[row * gridWidth + col]++;
    }

    // Exclusive prefix sum
    offsets.resize(gridWidth * gridWidth + 1);

    for (int i{1}; i < offsets.size(); ++i)
        offsets[i] = offsets[i - 1] + counts[i - 1];

    data.resize(offsets.back());
    std::vector<int> cursor(gridWidth * gridWidth, 0);

    // Second pass - populate the data vector with indicies
    for (std::size_t i{0}; i < meshVertices.size(); i += 3)
    {
        CellBounds bounds{getMinMax(meshVertices[i], meshVertices[i + 1], meshVertices[i + 2])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};

        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
            {
                int cellIndex{row * gridWidth + col};
                data[offsets[cellIndex] + cursor[cellIndex]] = static_cast<int>(i);
                ++cursor[cellIndex];
            }
    }
}

void SpatialGrid::query(const glm::vec3 &position, std::vector<int> &result)
{
    result.clear();

    int row{static_cast<int>((position.z - originZ) / cellSize)};
    int col{static_cast<int>((position.x - originX) / cellSize)};

    if (row < 0 || col < 0 || row >= gridWidth || col >= gridWidth)
        return;

    int cell{static_cast<int>(row * gridWidth + col)};

    for (int i{offsets[cell]}; i < offsets[cell + 1]; ++i)
        result.push_back(data[i]);
}