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
    std::vector<Primitive> takePrimitives();

  private:
    std::filesystem::path imagePath_;
    std::vector<Primitive> primitives_;
    bool primitivesTaken_{false};
};