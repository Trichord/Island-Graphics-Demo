#pragma once

#include <filesystem>
#include <vector>

#include "aabb.h"
#include "primitive.h"

class Skybox
{
  public:
    Skybox() = default;
    Skybox(const std::filesystem::path &imagePath);

    const AABB &getAABB() const;
    const std::filesystem::path &getPath() const;
    const std::vector<Primitive> &getPrimitives() const;

  private:
    AABB aabb_;
    std::filesystem::path image_;
    std::vector<Primitive> primitives_;
};