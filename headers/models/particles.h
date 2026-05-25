#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

class Particles
{
  public:
    Particles() = default;
    Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
              const glm::vec4 colorVariation, float lifetime, int numParticles);

    const AABB &getAABB() const;
    const std::vector<Primitive> &getPrimitives() const;

  private:
    AABB aabb_;
    std::vector<Primitive> primitives_;

    std::vector<Primitive> generateParticles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                                             const glm::vec4 colorVariation, float lifetime, int numParticles);
};