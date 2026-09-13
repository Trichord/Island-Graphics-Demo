#include "models/model.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"
#include "utilities/asset_loader.h"

Model::Model(const std::filesystem::path &path, ShaderType shader,
             std::optional<Primitive::VertexExplosion> vertexExplosion)
    : asset_{loader::loadAsset(path)}, filePath_{path}, primitives_{loader::loadMeshes(asset_)}
{
    for (Primitive &primitive : primitives_)
    {
        primitive.shaderType = shader;

        if (!vertexExplosion)
            continue;

        primitive.explosionDistances = vertexExplosion;

        std::vector<Primitive::Vertex> expandedTris;
        expandedTris.reserve(primitive.indices.size());

        for (std::uint32_t i{0}; i < primitive.indices.size(); ++i)
            expandedTris.emplace_back(primitive.vertices[primitive.indices[i]]);

        primitive.vertices = std::move(expandedTris);
        std::iota(primitive.indices.begin(), primitive.indices.end(), 0);

        primitive.aabb.setHalfExtent(primitive.aabb.getHalfExtent() + vertexExplosion->maxDistance);
    }
}

std::vector<Primitive> Model::takePrimitives()
{
    if (primitivesTaken_)
    {
        std::cerr << "Error: Primitives already taken for " << filePath_ << std::endl;
        throw std::runtime_error("Primitives already moved");
    }

    primitivesTaken_ = true;
    return std::move(primitives_);
}

const fastgltf::Asset &Model::getAsset() const
{
    return asset_;
}

const std::filesystem::path &Model::getPath() const
{
    return filePath_;
}