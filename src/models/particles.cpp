#include "models/particles.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

Particles::Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                     const glm::vec4 colorVariation, float lifeTime, int numParticles)
    : aabb_{position - extent, position + extent},
      primitives_{generateParticles(position, extent, color, colorVariation, std::min(lifeTime, 65504.0f), numParticles)}
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
                                                    float lifetime, int numParticles)
{
    constexpr float minLifetimePercent{0.50};

    auto randomFloat = [](float min, float max, std::mt19937 &mt) {
        if (min == max)
            return min;

        if (max < min)
            std::swap(max, min);

        std::uniform_real_distribution<float> rnd{min, max};
        return rnd(mt);
    };

    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<Primitive> primitives;
    primitives.resize(1);

    Primitive &primitive{primitives.front()};
    primitive.aabb = aabb_;
    primitive.alphaMode = AlphaMode::mask;
    primitive.indices.resize(numParticles);
    std::iota(primitive.indices.begin(), primitive.indices.end(), 0);
    primitive.vertices.resize(numParticles);

    std::uniform_real_distribution<float> randomPos{-1.0, 1.0};

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

        float red{std::clamp(randomFloat(redMin, redMax, mt), 0.0f, 1.0f)};
        float green{std::clamp(randomFloat(greenMin, greenMax, mt), 0.0f, 1.0f)};
        float blue{std::clamp(randomFloat(blueMin, blueMax, mt), 0.0f, 1.0f)};
        float alpha{std::clamp(randomFloat(alphaMin, alphaMax, mt), 0.0f, 1.0f)};

        float xPos{position.x + extent.x * randomPos(mt)};
        float yPos{position.y + extent.y * randomPos(mt)};
        float zPos{position.z + extent.z * randomPos(mt)};

        float xVel{randomPos(mt) * velocityModifier};
        float yVel{randomPos(mt) * velocityModifier};
        float zVel{randomPos(mt) * velocityModifier};

        vertex.position = glm::vec3{xPos, yPos, zPos};
        vertex.color = glm::packUnorm4x8(glm::vec4{red, green, blue, alpha});

        // Velocity stored in normal (x), tangent (y), texCoords (z)
        vertex.normal = glm::floatBitsToUint(xVel);
        vertex.tangent = glm::floatBitsToUint(yVel);
        vertex.texCoords = glm::floatBitsToUint(zVel);

        // Lifetime (max life time, current life remaining)
        glm::vec2 life{lifetime, randomFloat(lifetime * minLifetimePercent, lifetime, mt)};
        vertex.boneDataIndex = glm::packHalf2x16(life);
    }

    return primitives;
}