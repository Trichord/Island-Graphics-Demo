#include "models/skybox.h"

#include <cstdint>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

constexpr float skyboxSize{1e18f};

void setGeometry(std::vector<Primitive> &primitives, AABB &aabb);

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
        {.position = glm::vec3{-3.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{3.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{0.0f, 2.0f, 1.0f}},
    };

    std::vector<std::uint32_t> indices{0u, 1u, 2u};

    primitives = {Primitive{.vertices = std::move(vertices),
                            .indices = std::move(indices),
                            .aabb = aabb,
                            .materialIndex = std::size_t{0},
                            .alphaMode = fastgltf::AlphaMode::Opaque}};
}