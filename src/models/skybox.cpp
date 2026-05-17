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
std::vector<int> getMaterialIndices(const std::vector<Primitive> &primitives);

Skybox::Skybox(const std::filesystem::path &imagePath)
    : aabb_{glm::vec3{-skyboxSize}, glm::vec3{skyboxSize}}, image_{imagePath}
{
    setGeometry(primitives_, aabb_);
}

const AABB &Skybox::getAABB() const
{
    return aabb_;
}

const std::vector<Primitive> &Skybox::getPrimitives() const
{
    return primitives_;
}

const std::filesystem::path &Skybox::getPath() const
{
    return image_;
}

void setGeometry(std::vector<Primitive> &primitives, AABB &aabb)
{
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

std::vector<int> getMaterialIndices(const std::vector<Primitive> &primitives)
{
    std::vector<int> materialIndices;
    materialIndices.reserve(primitives.size());

    for (const Primitive &primitive : primitives)
        materialIndices.push_back(primitive.materialIndex);

    return materialIndices;
}