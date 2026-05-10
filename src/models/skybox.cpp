#include "skybox.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "primitive.h"
#include "texture_manager.h"

#include <iostream>

constexpr float skyboxSize{1e18f};

void setGeometry(std::vector<Primitive> &primitives, AABB &aabb);

Skybox::Skybox(TextureManager &textureManager, const std::filesystem::path &imagePath)
    : handle_{textureManager.assignHandle()}
{
    setGeometry(primitives_, aabb_);

    std::vector<int> materialIndices;
    materialIndices.reserve(primitives_.size());

    for (Primitive &primitive : primitives_)
        materialIndices.push_back(primitive.materialIndex);

    textureManager.loadCubemapTextures(handle_, imagePath, materialIndices);
}

Skybox::Skybox(TextureManager &textureManager, const Handle borrowHandle)
    : handle_{textureManager.assignHandle()}, borrowedHandle_{borrowHandle}
{
    setGeometry(primitives_, aabb_);
    textureManager.borrowCubemaps(handle_, borrowHandle);
}

AABB &Skybox::getAABB()
{
    return aabb_;
}

Handle Skybox::getHandle() const
{
    return handle_;
}

std::span<Primitive> Skybox::getPrimitives()
{
    return primitives_;
}

std::optional<Handle> Skybox::getBorrowedHandle() const
{
    return borrowedHandle_;
}

std::optional<std::vector<Primitive>> Skybox::loadCollisionMesh() const
{
    return {};
}

void setGeometry(std::vector<Primitive> &primitives, AABB &aabb)
{
    aabb = AABB(glm::vec3{-skyboxSize}, glm::vec3{skyboxSize});

    std::vector<Primitive::Vertex> vertices{
        {.position = glm::vec3{0.0f, 2.0f, 1.0f}},
        {.position = glm::vec3{3.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{-3.0f, -1.0f, 1.0f}},
    };
    std::vector<std::uint32_t> indices{0u, 2u, 1u};

    primitives = {Primitive{.vertices = std::move(vertices),
                            .indices = std::move(indices),
                            .aabb = aabb,
                            .materialIndex = 0,
                            .alphaMode = fastgltf::AlphaMode::Opaque}};
}