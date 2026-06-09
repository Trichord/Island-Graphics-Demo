#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "models/skeleton.h"
#include "utilities/texture_manager.h"

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
    std::optional<Skeleton> skeleton_;
    AABB aabb_;

    AABB calculateAABB() const;
};
