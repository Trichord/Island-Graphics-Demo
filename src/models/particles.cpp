#include "models/particles.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"
#include "utilities/random.h"

Particles::Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                     const glm::vec4 colorVariation, float lifetime, int numParticles)
    : primitives_{generateParticles(position, extent, color, colorVariation,
                                    std::isnan(lifetime) ? 65504.0f : std::min(lifetime, 65504.0f), numParticles)}
{
}

std::vector<Primitive> Particles::takePrimitives()
{
    if (primitivesTaken_)
    {
        std::cerr << "Error: Particle primitives already taken\n";
        throw std::runtime_error("Primitives already moved");
    }

    primitivesTaken_ = true;
    return std::move(primitives_);
}

std::vector<Primitive> Particles::generateParticles(const glm::vec3 &position, const glm::vec3 &extent,
                                                    const glm::vec4 &color, const glm::vec4 colorVariation,
                                                    float lifetime, int numParticles)
{
    constexpr float minLifetimePercent{0.50};
    constexpr float normalizedMin{-1.0f};
    constexpr float normalizedMax{1.0f};

    std::vector<Primitive> primitives{
        {.aabb = {position - extent, position + extent},
         .alphaMode = AlphaMode::mask,
         .shaderType = ShaderType::particles},
    };

    Primitive &primitive{primitives.front()};
    primitive.indices.resize(numParticles);
    std::iota(primitive.indices.begin(), primitive.indices.end(), 0);
    primitive.vertices.resize(numParticles);

    float redMin{(color.x - colorVariation.x) / 255.0f};
    float redMax{(color.x + colorVariation.x) / 255.0f};
    float greenMin{(color.y - colorVariation.y) / 255.0f};
    float greenMax{(color.y + colorVariation.y) / 255.0f};
    float blueMin{(color.z - colorVariation.z) / 255.0f};
    float blueMax{(color.z + colorVariation.z) / 255.0f};
    float alphaMin{(color.w - colorVariation.w) / 255.0f};
    float alphaMax{(color.w + colorVariation.w) / 255.0f};

    for (Primitive::Vertex &vertex : primitive.vertices)
    {
        constexpr float velocityModifier{5.0f};

        float red{std::clamp(random::floatMT(redMin, redMax), 0.0f, 1.0f)};
        float green{std::clamp(random::floatMT(greenMin, greenMax), 0.0f, 1.0f)};
        float blue{std::clamp(random::floatMT(blueMin, blueMax), 0.0f, 1.0f)};
        float alpha{std::clamp(random::floatMT(alphaMin, alphaMax), 0.0f, 1.0f)};

        float xPos{position.x + extent.x * random::floatMT(normalizedMin, normalizedMax)};
        float yPos{position.y + extent.y * random::floatMT(normalizedMin, normalizedMax)};
        float zPos{position.z + extent.z * random::floatMT(normalizedMin, normalizedMax)};

        float xVel{random::floatMT(normalizedMin, normalizedMax) * velocityModifier};
        float yVel{random::floatMT(normalizedMin, normalizedMax) * velocityModifier};
        float zVel{random::floatMT(normalizedMin, normalizedMax) * velocityModifier};

        vertex.position = glm::vec3{xPos, yPos, zPos};
        vertex.color = glm::packUnorm4x8(glm::vec4{red, green, blue, alpha});

        // Velocity stored in normal (x), tangent (y), texCoords (z)
        vertex.normal = glm::floatBitsToUint(xVel);
        vertex.tangent = glm::floatBitsToUint(yVel);
        vertex.texCoords = glm::floatBitsToUint(zVel);

        // Lifetime (max life time, current life remaining)
        glm::vec2 life{lifetime, random::floatMT(lifetime * minLifetimePercent, lifetime)};
        vertex.vboIndex = glm::packHalf2x16(life);
    }

    return primitives;
}