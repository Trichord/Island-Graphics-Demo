#include "buffer_builder.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "aabb.h"
#include "buffers.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderer_types.h"

namespace
{

struct SimpleAABB
{
    glm::vec4 center;
    glm::vec4 extent;
};

std::vector<SimpleAABB> simplifyAABBs(const std::vector<AABB> &aabbs)
{
    std::vector<SimpleAABB> simpleAABBs;
    simpleAABBs.reserve(aabbs.size());

    for (const AABB &aabb : aabbs)
        simpleAABBs.emplace_back(SimpleAABB{
            .center = glm::vec4{aabb.getCenter(), 0.0f},
            .extent = glm::vec4{aabb.getExtent(), 0.0f},
        });

    return simpleAABBs;
}

} // namespace

SSBOs BufferBuilder::buildSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                const std::vector<AABB> &aabbs, BufferBuilder::Sizes sizes) const
{
    using enum BindingPointSSBO;

    SSBOs buffers;
    std::vector<SimpleAABB> simpleAABBs{simplifyAABBs(aabbs)};
    std::size_t drawCommandsSize{commands.size() * sizeof(DrawElementsIndirectCommand)};

    buildSSBO(&buffers.map, sizes.bufferMap, map, true, nullptr);
    buildSSBO(&buffers.aabb, simpleAABBs.size() * sizeof(SimpleAABB), aabb, false, simpleAABBs.data());
    buildSSBO(&buffers.matrices, sizes.matrices, matrices, true, nullptr);
    buildSSBO(&buffers.metadata, sizes.metadata, metadata, true, nullptr);
    buildSSBO(&buffers.drawCounts, sizes.drawCounts, drawCounts, false, nullptr);
    buildSSBO(&buffers.cullResults, sizes.cullResults, cullResults, false, nullptr);
    buildSSBO(&buffers.drawCommands, drawCommandsSize, drawCommands, true, nullptr);
    buildSSBO(&buffers.visibleInstances, sizes.visibleInstances, visibleInstances, false, nullptr);
    buildSSBO(&buffers.drawCommandsSource, drawCommandsSize, drawCommandsSource, false, commands.data());

    return buffers;
}

UBOs BufferBuilder::buildUBOs()
{
    UBOs ubos;

    buildUBO(&ubos.camera, sizeof(PackedCameraUniforms), BindingPointUBO::camera);
    buildUBO(&ubos.cascadeShadows, sizeof(PackedCascadeShadows), BindingPointUBO::cascadeShadows);
    buildUBO(&ubos.frustumPlanes, 6 * sizeof(glm::vec4), BindingPointUBO::frustumPlanes);

    return ubos;
}

GlobalVAO BufferBuilder::buildVAO(const std::vector<Primitive::Vertex> &vertices,
                                  const std::vector<std::uint32_t> &indices) const
{
    GlobalVAO vao;

    glCreateVertexArrays(1, &vao.id);
    glBindVertexArray(vao.id);

    glEnableVertexArrayAttrib(vao.id, 0);
    glEnableVertexArrayAttrib(vao.id, 1);
    glEnableVertexArrayAttrib(vao.id, 2);
    glEnableVertexArrayAttrib(vao.id, 3);

    glCreateBuffers(1, &vao.vbo);
    glNamedBufferStorage(vao.vbo, vertices.size() * sizeof(Primitive::Vertex), vertices.data(), 0);

    glCreateBuffers(1, &vao.ebo);
    glNamedBufferStorage(vao.ebo, indices.size() * sizeof(std::uint32_t), indices.data(), 0);

    glVertexArrayVertexBuffer(vao.id, 0, vao.vbo, 0, sizeof(Primitive::Vertex));

    glVertexArrayElementBuffer(vao.id, vao.ebo);

    glVertexArrayAttribFormat(vao.id, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, position));
    glVertexArrayAttribFormat(vao.id, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, normal));
    glVertexArrayAttribFormat(vao.id, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, tangent));
    glVertexArrayAttribFormat(vao.id, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, texCoords));

    glVertexArrayAttribBinding(vao.id, 0, 0);
    glVertexArrayAttribBinding(vao.id, 1, 0);
    glVertexArrayAttribBinding(vao.id, 2, 0);
    glVertexArrayAttribBinding(vao.id, 3, 0);

    return vao;
}

void BufferBuilder::buildSSBO(GLuint *id, std::size_t size, BindingPointSSBO binding, bool dynamic,
                              const void *data) const
{
    GLenum storageType{static_cast<GLenum>(dynamic ? GL_DYNAMIC_STORAGE_BIT : 0)};
    glCreateBuffers(1, id);
    glNamedBufferStorage(*id, static_cast<GLsizeiptr>(size), data, storageType);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(binding), *id);
}

void BufferBuilder::buildUBO(GLuint *id, std::size_t size, BindingPointUBO binding) const
{
    glCreateBuffers(1, id);
    glNamedBufferStorage(*id, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(binding), *id);
}