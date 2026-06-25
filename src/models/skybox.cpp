#include "models/skybox.h"

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

constexpr float skyboxSize{1e18f};

Skybox::Skybox(const std::filesystem::path &imagePath) : image_{imagePath}
{
    // Make the skybox have a massive AABB so it never gets culled
    AABB aabb{glm::vec3{-skyboxSize}, glm::vec3{skyboxSize}};

    std::vector<Primitive::Vertex> vertices{
        {.position = glm::vec3{-3.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{3.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{0.0f, 2.0f, 1.0f}},
    };

    std::vector<std::uint32_t> indices{0u, 1u, 2u};

    primitives_ = {Primitive{.vertices = std::move(vertices),
                             .indices = std::move(indices),
                             .aabb = aabb,
                             .materialIndex = std::size_t{0},
                             .alphaMode = AlphaMode::opaque}};
}

const std::vector<Primitive> &Skybox::getPrimitives() const
{
    return primitives_;
}

const std::filesystem::path &Skybox::getPath() const
{
    return image_;
}
