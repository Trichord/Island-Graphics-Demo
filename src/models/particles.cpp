#include "particles.h"

#include <numeric>
#include <random>
#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"
#include "primitive.h"

Particles::Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                     const glm::vec4 colorVariation, int numParticles)
    : aabb_{position - extent, position + extent},
      primitives_{generateParticles(position, extent, color, colorVariation, numParticles)}
{
}

const AABB &Particles::getAABB() const
{
    return aabb_;
}

const std::vector<Primitive> &Particles::getPrimitives() const
{
    return primitives_;
}

std::vector<Primitive> Particles::generateParticles(const glm::vec3 &position, const glm::vec3 &extent,
                                                    const glm::vec4 &color, const glm::vec4 colorVariation,
                                                    int numParticles)
{
    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<Primitive> primitives;
    primitives.resize(1);

    Primitive &primitive{primitives.front()};
    primitive.aabb = aabb_;
    primitive.alphaMode = fastgltf::AlphaMode::Mask;
    primitive.indices.resize(numParticles);
    std::iota(primitive.indices.begin(), primitive.indices.end(), 0);
    primitive.vertices.resize(numParticles);

    std::uniform_real_distribution<float> randomRed{std::clamp((color.x - colorVariation.x) / 255.0f, 0.0f, 1.0f),
                                                    std::clamp((color.x + colorVariation.x) / 255.0f, 0.0f, 1.0f)};
    std::uniform_real_distribution<float> randomGreen{std::clamp((color.y - colorVariation.y) / 255.0f, 0.0f, 1.0f),
                                                      std::clamp((color.y + colorVariation.y) / 255.0f, 0.0f, 1.0f)};
    std::uniform_real_distribution<float> randomBlue{std::clamp((color.z - colorVariation.z) / 255.0f, 0.0f, 1.0f),
                                                     std::clamp((color.z + colorVariation.z) / 255.0f, 0.0f, 1.0f)};
    std::uniform_real_distribution<float> randomAlpha{std::clamp((color.w - colorVariation.w) / 255.0f, 0.0f, 1.0f),
                                                      std::clamp((color.w + colorVariation.w) / 255.0f, 0.0f, 1.0f)};

    std::uniform_real_distribution<float> randomPos{-1.0, 1.0};
    std::uniform_real_distribution<float> randomBlackHolePos{0.25f, 0.5f};
    std::uniform_real_distribution<float> otherBlackHoleOffset{0.0f, 0.25f};

    for (Primitive::Vertex &vertex : primitive.vertices)
    {
        constexpr float velocityModifier{5.0f};

        float red{randomRed(mt)};
        float green{randomGreen(mt)};
        float blue{randomBlue(mt)};
        float alpha{randomAlpha(mt)};

        float xPos{position.x + extent.x * randomPos(mt)};
        float yPos{position.y + extent.y * randomPos(mt)};
        float zPos{position.z + extent.z * randomPos(mt)};

        float xVel{randomPos(mt) * velocityModifier};
        float yVel{randomPos(mt) * velocityModifier};
        float zVel{randomPos(mt) * velocityModifier};

        vertex.position = glm::vec3{xPos, yPos, zPos};
        vertex.normal = glm::vec3{xVel, yVel, zVel};
        vertex.tangent = glm::vec4{red, green, blue, alpha};
    }

    return primitives;
}