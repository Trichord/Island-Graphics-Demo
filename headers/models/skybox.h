#pragma once

#include <filesystem>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

class Skybox
{
  public:
    Skybox() = default;
    Skybox(const std::filesystem::path &imagePath);

    const std::filesystem::path &getPath() const;
    const std::vector<Primitive> &getPrimitives() const;

  private:
    std::filesystem::path imagePath_;
    std::vector<Primitive> primitives_;
};