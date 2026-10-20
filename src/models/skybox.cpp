#include "models/skybox.h"

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"

Skybox::Skybox(const std::filesystem::path &imagePath) : imagePath_{imagePath}
{
    constexpr float skyboxSize{1e18f};

    // Make the skybox have a massive AABB so it never gets culled
    AABB aabb{glm::vec3{-skyboxSize}, glm::vec3{skyboxSize}};

    std::vector<Primitive::Vertex> vertices{
        {.position = glm::vec3{-1.0f, 3.0f, 1.0f}},
        {.position = glm::vec3{-1.0f, -1.0f, 1.0f}},
        {.position = glm::vec3{3.0f, -1.0f, 1.0f}},
    };

    std::vector<std::uint32_t> indices{0u, 1u, 2u};

    primitives_ = {Primitive{
        .vertices = std::move(vertices),
        .indices = std::move(indices),
        .aabb = aabb,
        .materialIndex = std::size_t{0},
        .alphaMode = AlphaMode::opaque,
        .shaderType = ShaderType::skybox,
    }};
}

std::vector<Primitive> Skybox::takePrimitives()
{
    if (primitivesTaken_)
    {
        std::cerr << "Error: Skybox primitives already taken\n";
        throw std::runtime_error("Primitives already moved");
    }

    primitivesTaken_ = true;
    return std::move(primitives_);
}

const std::filesystem::path &Skybox::getPath() const
{
    return imagePath_;
}
