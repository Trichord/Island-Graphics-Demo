#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation/animation.h"
#include "models/animation/node_graph.h"
#include "models/animation/skeleton.h"
#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"

class Model
{
  public:
    Model(const std::filesystem::path &path, ShaderType shader,
          std::optional<Primitive::VertexExplosion> vertexExplosion = std::nullopt);

    const fastgltf::Asset &getAsset() const;
    const std::filesystem::path &getPath() const;

    std::vector<Primitive> takePrimitives();

  protected:
    fastgltf::Asset asset_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;

    bool primitivesTaken_{false};
};
