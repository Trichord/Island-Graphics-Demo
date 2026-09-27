#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"

/**
 * @brief A cloud of colored particles, smuggled inside the 32 byte vertex. Vertex position and color remain true to
 * their name. The rest of the vertex is repurposed into a full float velocity and packed particle lifetime (max
 * lifetime / time remaining).
 */
class Particles
{
  public:
    Particles() = default;
    Particles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
              const glm::vec4 colorVariation, float lifetime, int numParticles);

    std::vector<Primitive> takePrimitives();

  private:
    std::vector<Primitive> primitives_;
    bool primitivesTaken_{false};

    std::vector<Primitive> generateParticles(const glm::vec3 &position, const glm::vec3 &extent, const glm::vec4 &color,
                                             const glm::vec4& colorVariation, float lifetime, int numParticles);
};