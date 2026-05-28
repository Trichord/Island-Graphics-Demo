#include "utilities/buffer_builder.h"

#include <cstddef>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/renderer_types.h"

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
                                const std::vector<glm::mat4> &matrices, const std::vector<PrimitiveMetadata> &metadata,
                                const std::vector<VisibilityBufferMap> &visibilityMap,
                                const std::vector<ParticleBufferMap> &particleMap, const std::vector<AABB> &aabbs,
                                BufferBuilder::Sizes sizes) const
{
    SSBOs buffers;
    std::vector<SimpleAABB> simpleAABBs{simplifyAABBs(aabbs)};
    std::size_t drawCommandsSize{commands.size() * sizeof(DrawElementsIndirectCommand)};

    buildSSBO(&buffers.visibilityMap, sizes.bufferMap, BindingPointSSBO::visibilityMap, false, visibilityMap.data());
    buildSSBO(&buffers.aabb, simpleAABBs.size() * sizeof(SimpleAABB), BindingPointSSBO::aabb, false,
              simpleAABBs.data());
    buildSSBO(&buffers.matrices, sizes.matrices, BindingPointSSBO::matrices, true, matrices.data());
    buildSSBO(&buffers.metadata, sizes.metadata, BindingPointSSBO::metadata, true, metadata.data());
    buildSSBO(&buffers.drawCounts, sizes.drawCounts, BindingPointSSBO::drawCounts, false, nullptr);
    buildSSBO(&buffers.cullResults, sizes.cullResults, BindingPointSSBO::cullResults, false, nullptr);
    buildSSBO(&buffers.drawCommands, drawCommandsSize, BindingPointSSBO::drawCommands, true, nullptr);
    buildSSBO(&buffers.visibleInstances, sizes.visibleInstances, BindingPointSSBO::visibleInstances, false, nullptr);
    buildSSBO(&buffers.drawCommandsSource, drawCommandsSize, BindingPointSSBO::drawCommandsSource, false,
              commands.data());
    buildSSBO(&buffers.particleMap, particleMap.size() * sizeof(ParticleBufferMap), BindingPointSSBO::particleMap,
              false, particleMap.data());

    return buffers;
}

UBOs BufferBuilder::buildUBOs()
{
    UBOs ubos;

    buildUBO(&ubos.camera, sizeof(PackedCameraUniforms), BindingPointUBO::camera);
    buildUBO(&ubos.cascadeShadows, sizeof(PackedCascadeShadows), BindingPointUBO::cascadeShadows);
    buildUBO(&ubos.frustumPlanes, 6 * sizeof(glm::vec4), BindingPointUBO::frustumPlanes);
    buildUBO(&ubos.time, sizeof(PackedTime), BindingPointUBO::time);

    return ubos;
}

VAOs BufferBuilder::buildVAO(const std::vector<Primitive::Vertex> &vertices,
                             const std::vector<std::uint32_t> &indices) const
{
    VAOs vao;

    glCreateVertexArrays(1, &vao.vao);

    glEnableVertexArrayAttrib(vao.vao, 0);
    glEnableVertexArrayAttrib(vao.vao, 1);
    glEnableVertexArrayAttrib(vao.vao, 2);
    glEnableVertexArrayAttrib(vao.vao, 3);

    glCreateBuffers(1, &vao.vbo);
    glNamedBufferStorage(vao.vbo, vertices.size() * sizeof(Primitive::Vertex), vertices.data(), 0);

    glCreateBuffers(1, &vao.ebo);
    glNamedBufferStorage(vao.ebo, indices.size() * sizeof(std::uint32_t), indices.data(), 0);

    glVertexArrayVertexBuffer(vao.vao, 0, vao.vbo, 0, sizeof(Primitive::Vertex));
    glVertexArrayElementBuffer(vao.vao, vao.ebo);

    glVertexArrayAttribFormat(vao.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, position));
    glVertexArrayAttribFormat(vao.vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, normal));
    glVertexArrayAttribFormat(vao.vao, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, tangent));
    glVertexArrayAttribFormat(vao.vao, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, texCoords));

    glVertexArrayAttribBinding(vao.vao, 0, 0);
    glVertexArrayAttribBinding(vao.vao, 1, 0);
    glVertexArrayAttribBinding(vao.vao, 2, 0);
    glVertexArrayAttribBinding(vao.vao, 3, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::vbo), vao.vbo);

    // Shadow VAO
    struct ShadowVertex
    {
        glm::vec4 pos{0.0f};
        glm::vec2 uv{0.0f};
    };

    std::vector<ShadowVertex> shadowVerts;
    shadowVerts.reserve(vertices.size());

    for (const Primitive::Vertex &v : vertices)
        shadowVerts.emplace_back(ShadowVertex{.pos = glm::vec4{v.position, 1.0f}, .uv = v.texCoords});

    glCreateVertexArrays(1, &vao.shadowVAO);

    glEnableVertexArrayAttrib(vao.shadowVAO, 0);
    glEnableVertexArrayAttrib(vao.shadowVAO, 1);

    glCreateBuffers(1, &vao.shadowVBO);
    glNamedBufferStorage(vao.shadowVBO, vertices.size() * sizeof(ShadowVertex), shadowVerts.data(), 0);

    glVertexArrayVertexBuffer(vao.shadowVAO, 0, vao.shadowVBO, 0, sizeof(ShadowVertex));
    glVertexArrayElementBuffer(vao.shadowVAO, vao.ebo);

    glVertexArrayAttribFormat(vao.shadowVAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(ShadowVertex, pos));
    glVertexArrayAttribFormat(vao.shadowVAO, 1, 2, GL_FLOAT, GL_FALSE, offsetof(ShadowVertex, uv));

    glVertexArrayAttribBinding(vao.shadowVAO, 0, 0);
    glVertexArrayAttribBinding(vao.shadowVAO, 1, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::shadowVBO), vao.shadowVBO);

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