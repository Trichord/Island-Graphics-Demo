#pragma once

#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"
#include "primitive.h"

class Particles
{
  public:
    Particles() = default;
    Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color, const glm::vec4 colorVariation,
              int numParticles);

    const AABB &getAABB() const;
    const std::vector<Primitive> &getPrimitives() const;

  private:
    AABB aabb_;
    std::vector<Primitive> primitives_;

    std::vector<Primitive> generateParticles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                                             const glm::vec4 colorVariation, int numParticles);
};