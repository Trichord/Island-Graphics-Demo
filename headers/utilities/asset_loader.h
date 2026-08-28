#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <vector>

#include "models/animation/animation.h"
#include "models/animation/node_graph.h"
#include "models/animation/skeleton.h"
#include "models/primitive.h"
#include "renderer/textures.h"
#include "scene/handle.h"

namespace loader
{

struct TextureDescriptor
{
    Sampler sampler;
    std::size_t imageIndex{0};
};

struct DeferredTexture
{
    Handle handle{0};
    std::filesystem::path path;

    bool textureRepeat{true};
    std::vector<std::size_t> materialIndices;
    Material::Type type{Material::Type::albedo};
};

fastgltf::Asset loadAsset(const std::filesystem::path &path);
std::vector<Primitive> loadMeshes(const fastgltf::Asset &asset);
std::vector<std::filesystem::path> getURIs(const fastgltf::Asset &asset, const std::filesystem::path &parentPath);
TextureDescriptor getTextureInfo(const fastgltf::Asset &asset, const fastgltf::TextureInfo &texInfo);
std::vector<DeferredTexture> texturesFromAsset(const fastgltf::Asset &asset, const std::filesystem::path &parentPath,
                                               Handle handle);
NodeGraph loadNodeGraph(const fastgltf::Asset &asset);
std::optional<Skeleton> loadSkeleton(const fastgltf::Asset &asset);
std::vector<Animation> loadAnimations(const fastgltf::Asset &asset);
void reorderGraph(NodeGraph &graph, std::vector<Animation> &animations, std::optional<Skeleton> &skeleton);
} // namespace loader