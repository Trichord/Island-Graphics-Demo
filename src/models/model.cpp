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
        if (primitive.vertexSkins && vertexExplosion)
        {
            std::cerr << "Error: Cannot explode animated models\n";
            throw std::runtime_error("Cannot explode animated models");
        }

        primitive.shaderType = shader;

        if (!vertexExplosion)
            continue;

        primitive.explosionDistances = vertexExplosion;

        // Expand all indices to eliminate shared verts. For the explosion effect, each triangle must be distinct with
        // its own geometric normal.
        std::vector<Primitive::Vertex> expandedTris;
        expandedTris.reserve(primitive.indices.size());
        for (std::uint32_t i{0}; i < primitive.indices.size(); ++i)
            expandedTris.emplace_back(primitive.vertices[primitive.indices[i]]);

        // Rewrite indices to match new verts (identity indices, i.e. 0, 1, 2, ..., n)
        primitive.vertices = std::move(expandedTris);
        std::iota(primitive.indices.begin(), primitive.indices.end(), 0);

        // Fix AABB so its not culled at max explosion size
        primitive.aabb.setHalfExtent(primitive.aabb.getHalfExtent() + vertexExplosion->maxDistance);

        // Since we can now see the back sides of triangles when exploded, force the alpha mode
        primitive.alphaMode = AlphaMode::mask;
    }
}

std::vector<Primitive> Model::takePrimitives()
{
    if (primitivesTaken_)
    {
        std::cerr << "Error: Primitives already taken for " << filePath_ << "\n";
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