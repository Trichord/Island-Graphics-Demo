#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation/skeletal_animation.h"
#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"

class Model
{
  public:
    Model(const std::filesystem::path &path, ShaderType shader);

    const std::filesystem::path &getPath() const;
    const fastgltf::Asset &getAsset() const;
    const std::vector<Primitive> &getPrimitives() const;
    const std::optional<SkeletalAnimation> &getSkeletalAnimation() const;

  protected:
    fastgltf::Asset asset_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;
    std::optional<SkeletalAnimation> skeletalAnimation_;

    void recalculateAnimatedAABBs();
};
