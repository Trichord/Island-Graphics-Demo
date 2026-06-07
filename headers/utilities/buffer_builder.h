#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/renderer_types.h"

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
        std::size_t drawCommandsSize{0};
    };

    [[nodiscard]] VAOs buildVAO(const std::vector<Primitive::Vertex> &vertices,
                                     const std::vector<std::uint32_t> &indices) const;

    [[nodiscard]] SSBOs buildSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                   const std::vector<glm::mat4> &matrices,
                                   const std::vector<PrimitiveMetadata> &metadata,
                                   const std::vector<VisibilityBufferMap> &visibilityMap,
                                   const std::vector<VertexBufferMap> &vertexMap, const std::vector<AABB> &aabbs,
                                   BufferBuilder::Sizes sizes) const;

    [[nodiscard]] UBOs buildUBOs();

  private:
    void buildUBO(GLuint *id, std::size_t size, BindingPointUBO binding) const;
    void buildSSBO(GLuint *id, std::size_t size, BindingPointSSBO binding, bool dynamic, const void *data) const;
};