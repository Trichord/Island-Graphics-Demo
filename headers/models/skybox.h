#pragma once

#include <filesystem>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

/**
 * @brief A screen space triangle with massive (nearly uncullable) AABB, designed to fit into the GPU driven pipeline
 * without special draw calls.
 */
class Skybox
{
  public:
    Skybox() = default;
    Skybox(const std::filesystem::path &imagePath);

    const std::filesystem::path &getPath() const;
    std::vector<Primitive> takePrimitives();

  private:
    std::filesystem::path imagePath_;
    std::vector<Primitive> primitives_;
    bool primitivesTaken_{false};
};