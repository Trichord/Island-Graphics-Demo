#pragma once

#include <cstdint>
#include <vector>

#include "aabb.h"
#include "buffers.h"
#include "glad/glad.h"
#include "primitive.h"
#include "renderer_types.h"

class BufferBuilder
{
  public:
    // Buffer size in bytes
    struct Sizes
    {
        std::size_t metadata{0};
        std::size_t matrices{0};
        std::size_t bufferMap{0};
        std::size_t drawCounts{0};
        std::size_t cullResults{0};
        std::size_t visibleInstances{0};
    };

    [[nodiscard]] GlobalVAO buildVAO(const std::vector<Primitive::Vertex> &vertices,
                                     const std::vector<std::uint32_t> &indices) const;

    [[nodiscard]] SSBOs buildSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                   const std::vector<glm::mat4> &matrices,
                                   const std::vector<PrimitiveMetadata> &metadata,
                                   const std::vector<VisibilityBufferMap> &visibilityMap,
                                   const std::vector<ParticleBufferMap> &particleMap, const std::vector<AABB> &aabbs,
                                   BufferBuilder::Sizes sizes) const;

    [[nodiscard]] UBOs buildUBOs();

  private:
    void buildUBO(GLuint *id, std::size_t size, BindingPointUBO binding) const;
    void buildSSBO(GLuint *id, std::size_t size, BindingPointSSBO binding, bool dynamic, const void *data) const;
};