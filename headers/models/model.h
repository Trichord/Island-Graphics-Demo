#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation.h"
#include "models/primitive.h"
#include "models/skeleton.h"

class Model
{
  public:
    Model(const std::filesystem::path &path);

    const std::filesystem::path &getPath() const;
    const fastgltf::Asset &getAsset() const;
    const std::vector<Primitive> &getPrimitives() const;
    const std::optional<Skeleton> &getSkeleton() const;
    const std::optional<std::vector<Animation>> &getAnimations() const;

  protected:
    fastgltf::Asset asset_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;
    std::optional<Skeleton> skeleton_;
    std::optional<std::vector<Animation>> animations_;

    void recalculateAnimatedAABBs();
};
