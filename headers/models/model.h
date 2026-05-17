#pragma once

#include <filesystem>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "primitive.h"
#include "texture_manager.h"

class Model
{
  public:
    Model(const std::filesystem::path &path);

    const AABB &getAABB() const;
    const std::filesystem::path &getPath() const;
    const fastgltf::Asset &getAsset() const;
    const std::vector<Primitive> &getPrimitives() const;

  protected:
    fastgltf::Asset asset_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;
    AABB aabb_;

    AABB calculateAABB() const;
};
