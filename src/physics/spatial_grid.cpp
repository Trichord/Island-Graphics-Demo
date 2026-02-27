#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <iostream>

#include "aabb.h"
#include "glm/glm.hpp"
#include "spatial_grid.h"

// Two-pass construction as described by Christer Ericson
SpatialGrid::SpatialGrid(const AABB &worldBounds, float cellSize, const std::vector<glm::vec3> &terrainVertices)
    : cellSize_{cellSize}, originX_{worldBounds.getMinX()}, originZ_{worldBounds.getMinZ()}
{
    if (terrainVertices.empty())
        return;

    // Force a square grid based of world AABB width
    gridWidth_ = static_cast<int>(std::ceilf(worldBounds.getMaxX() - worldBounds.getMinX()) / cellSize);
    gridHeight_ = static_cast<int>(std::ceilf(worldBounds.getMaxZ() - worldBounds.getMinZ()) / cellSize);

    gridWidth_ = std::max(1, gridWidth_);
    gridHeight_ = std::max(1, gridHeight_);

    int totalCells{gridWidth_ * gridHeight_};

    std::vector<int> counts(totalCells, 0);

    // First pass - count how many cells each triangle's AABB overlaps
    for (std::size_t i{0}; i < terrainVertices.size(); i += 3)
    {
        CellBounds bounds{getMinMaxBounds(terrainVertices[i], terrainVertices[i + 1], terrainVertices[i + 2])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};

        // Count touching cells
        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
                counts[row * gridWidth_ + col]++;
    }

    // Exclusive prefix sum
    offsets_.resize(gridWidth_ * gridHeight_ + 1);

    for (int i{1}; i < offsets_.size(); ++i)
        offsets_[i] = offsets_[i - 1] + counts[i - 1];

    data_.resize(offsets_.back());
    std::vector<int> cursor(gridWidth_ * gridHeight_, 0);

    // Second pass - populate the data vector with indicies
    for (std::size_t i{0}; i < terrainVertices.size(); i += 3)
    {
        CellBounds bounds{getMinMaxBounds(terrainVertices[i], terrainVertices[i + 1], terrainVertices[i + 2])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};

        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
            {
                int cellIndex{row * gridWidth_ + col};
                data_[offsets_[cellIndex] + cursor[cellIndex]] = static_cast<int>(i);
                ++cursor[cellIndex];
            }
    }
}

SpatialGrid::SpatialGrid(const AABB &worldBounds, float cellSize, const std::vector<AABB> AABBs)
{
    if (AABBs.empty())
        return;

    gridWidth_ = static_cast<int>(std::ceilf(worldBounds.getMaxX() - worldBounds.getMinX()) / cellSize);

    std::vector<int> counts(gridWidth_ * gridWidth_, 0);

    // First pass
    for (std::size_t i{0}; i < AABBs.size(); ++i)
    {
        CellBounds bounds{getMinMaxBounds(AABBs[i])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};

        // Count touching cells
        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
                counts[row * gridWidth_ + col]++;
    }

    offsets_.resize(gridWidth_ * gridWidth_ + 1);

    for (int i{1}; i < offsets_.size(); ++i)
        offsets_[i] = offsets_[i - 1] + counts[i - 1];

    data_.resize(offsets_.back());
    std::vector<int> cursor(gridWidth_ * gridWidth_, 0);

    // Second pass
    for (std::size_t i{0}; i < AABBs.size(); ++i)
    {
        CellBounds bounds{getMinMaxBounds(AABBs[i])};

        int cellMinX{bounds.minX};
        int cellMaxX{bounds.maxX};
        int cellMinZ{bounds.minZ};
        int cellMaxZ{bounds.maxZ};


        for (int row{cellMinZ}; row <= cellMaxZ; ++row)
            for (int col{cellMinX}; col <= cellMaxX; ++col)
            {
                int cellIndex{row * gridWidth_ + col};
                data_[offsets_[cellIndex] + cursor[cellIndex]] = static_cast<int>(i);
                ++cursor[cellIndex];
            }
    }
}

void SpatialGrid::query(const glm::vec3 &position, std::vector<int> &result)
{
    result.clear();

    int row{static_cast<int>((position.z - originZ_) / cellSize_)};
    int col{static_cast<int>((position.x - originX_) / cellSize_)};
    
    row = std::clamp(row, 0, gridHeight_ - 1);
    col = std::clamp(col, 0, gridWidth_ - 1);

    int cell{static_cast<int>(row * gridWidth_ + col)};

    for (int i{offsets_[cell]}; i < offsets_[cell + 1]; ++i)
        result.push_back(data_[i]);
}

SpatialGrid::CellBounds SpatialGrid::getMinMaxBounds(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C)
{
    float minX{std::min({A.x, B.x, C.x})};
    float minZ{std::min({A.z, B.z, C.z})};
    float maxX{std::max({A.x, B.x, C.x})};
    float maxZ{std::max({A.z, B.z, C.z})};

    // Shifted by origin
    int cellMinX{static_cast<int>((minX - originX_) / cellSize_)};
    int cellMaxX{static_cast<int>((maxX - originX_) / cellSize_)};
    int cellMinZ{static_cast<int>((minZ - originZ_) / cellSize_)};
    int cellMaxZ{static_cast<int>((maxZ - originZ_) / cellSize_)};

    cellMinX = std::clamp(cellMinX, 0, gridWidth_ - 1);
    cellMaxX = std::clamp(cellMaxX, 0, gridWidth_ - 1);
    cellMinZ = std::clamp(cellMinZ, 0, gridHeight_ - 1);
    cellMaxZ = std::clamp(cellMaxZ, 0, gridHeight_ - 1);

    return CellBounds{cellMinX, cellMaxX, cellMinZ, cellMaxZ};
}

SpatialGrid::CellBounds SpatialGrid::getMinMaxBounds(const AABB &aabb)
{
    float minX{aabb.getMinX()};
    float minZ{aabb.getMinZ()};
    float maxX{aabb.getMaxX()};
    float maxZ{aabb.getMaxZ()};

    int cellMinX{static_cast<int>((minX - originX_) / cellSize_)};
    int cellMaxX{static_cast<int>((maxX - originX_) / cellSize_)};
    int cellMinZ{static_cast<int>((minZ - originZ_) / cellSize_)};
    int cellMaxZ{static_cast<int>((maxZ - originZ_) / cellSize_)};

    cellMinX = std::clamp(cellMinX, 0, gridWidth_ - 1);
    cellMaxX = std::clamp(cellMaxX, 0, gridWidth_ - 1);
    cellMinZ = std::clamp(cellMinZ, 0, gridHeight_ - 1);
    cellMaxZ = std::clamp(cellMaxZ, 0, gridHeight_ - 1);

    return CellBounds{cellMinX, cellMaxX, cellMinZ, cellMaxZ};
}