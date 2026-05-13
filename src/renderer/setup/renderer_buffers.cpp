#include "renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include "aabb.h"
#include "cascade_shadow.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "primitive.h"
#include "renderer_types.h"
#include "setup_functions.h"
#include "textures.h"
#include "transform.h"

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

void Renderer::createBuffers(const std::vector<DrawElementsIndirectCommand> &commands, const std::vector<AABB> &aabbs,
                             const SceneTotals &totals)
{
    std::vector<SimpleAABB> simpleAABBs{simplifyAABBs(aabbs)};
    GLsizeiptr drawCommandSize{static_cast<GLsizeiptr>(commands.size())};

    // UBOs
    glCreateBuffers(1, &shaderUniformsUBO_);
    glNamedBufferStorage(shaderUniformsUBO_, sizeof(PackedShaderUniforms), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(BindingPointUBO::shaderUniforms), shaderUniformsUBO_);

    glCreateBuffers(1, &shadowCascadesUBO_);
    glNamedBufferStorage(shadowCascadesUBO_, sizeof(PackedCascadeShadows), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(BindingPointUBO::cascadeShadows), shadowCascadesUBO_);

    glCreateBuffers(1, &frustumPlanesUBO_);
    glNamedBufferStorage(frustumPlanesUBO_, 6 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(BindingPointUBO::frustumPlanes), frustumPlanesUBO_);

    // Buffer Maps
    glCreateBuffers(1, &mapBuffer_);
    glNamedBufferStorage(mapBuffer_, totals.visibleInstances * sizeof(BufferMap), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::map), mapBuffer_);

    // AABBs
    glCreateBuffers(1, &aabbBuffer_);
    glNamedBufferStorage(aabbBuffer_, simpleAABBs.size() * sizeof(SimpleAABB), simpleAABBs.data(), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::aabb), aabbBuffer_);

    // Instanced model matrices
    glCreateBuffers(1, &matricesBuffer_);
    glNamedBufferStorage(matricesBuffer_, totals.matrices * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::matrices), matricesBuffer_);

    // Metadata
    glCreateBuffers(1, &metadataBuffer_);
    glNamedBufferStorage(metadataBuffer_, drawCommandSize * sizeof(PrimitiveMetadata), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::metadata), metadataBuffer_);

    // Draw Counts (drawcount for glMultiDrawElementsIndirectCount)
    glCreateBuffers(1, &drawCountsBuffer_);
    glNamedBufferStorage(drawCountsBuffer_, static_cast<GLuint>(drawBuckets_.size() * sizeof(std::uint32_t)), nullptr,
                         0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::drawCounts), drawCountsBuffer_);
    glBindBuffer(GL_PARAMETER_BUFFER, drawCountsBuffer_);

    // Cull results
    glCreateBuffers(1, &cullResultsBuffer_);
    glNamedBufferStorage(cullResultsBuffer_, drawCommandSize * sizeof(std::uint32_t), nullptr, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::cullResults), cullResultsBuffer_);

    // Draw command buffer (dynamic)
    glCreateBuffers(1, &drawCommandsBuffer_);
    glNamedBufferStorage(drawCommandsBuffer_, drawCommandSize * sizeof(DrawElementsIndirectCommand), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::drawCommands),
                     drawCommandsBuffer_);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCommandsBuffer_);

    // Indices into the model matrices buffer. Each primitive has its own set of visible indicies, with the number of
    // model matrices being the upper bound
    glCreateBuffers(1, &visibleInstancesBuffer_);
    glNamedBufferStorage(visibleInstancesBuffer_, totals.visibleInstances * sizeof(std::uint32_t), nullptr, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::visibleInstances),
                     visibleInstancesBuffer_);

    // Draw command buffer (unchanging)
    glCreateBuffers(1, &drawCommandsSourceBuffer_);
    glNamedBufferStorage(drawCommandsSourceBuffer_, drawCommandSize * sizeof(DrawElementsIndirectCommand),
                         commands.data(), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(BindingPointSSBO::drawCommandsSource),
                     drawCommandsSourceBuffer_);
}

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */
void Renderer::createVAO(const std::vector<DrawEntry> &entries, const SceneTotals &stats)
{
    glCreateVertexArrays(1, &globalVAO_);
    glBindVertexArray(globalVAO_);

    glEnableVertexArrayAttrib(globalVAO_, 0);
    glEnableVertexArrayAttrib(globalVAO_, 1);
    glEnableVertexArrayAttrib(globalVAO_, 2);
    glEnableVertexArrayAttrib(globalVAO_, 3);

    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(stats.vertices);
    indices.reserve(stats.indices);

    for (const DrawEntry &entry : entries)
    {
        std::copy(entry.primitive->vertices.begin(), entry.primitive->vertices.end(), std::back_inserter(vertices));
        std::copy(entry.primitive->indices.begin(), entry.primitive->indices.end(), std::back_inserter(indices));
    }

    glCreateBuffers(1, &globalVBO_);
    glNamedBufferStorage(globalVBO_, vertices.size() * sizeof(Primitive::Vertex), vertices.data(), 0);

    glCreateBuffers(1, &globalEBO_);
    glNamedBufferStorage(globalEBO_, indices.size() * sizeof(std::uint32_t), indices.data(), 0);

    glVertexArrayVertexBuffer(globalVAO_, 0, globalVBO_, 0, sizeof(Primitive::Vertex));

    glVertexArrayElementBuffer(globalVAO_, globalEBO_);

    glVertexArrayAttribFormat(globalVAO_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, position));
    glVertexArrayAttribFormat(globalVAO_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, normal));
    glVertexArrayAttribFormat(globalVAO_, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, tangent));
    glVertexArrayAttribFormat(globalVAO_, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, texCoords));

    glVertexArrayAttribBinding(globalVAO_, 0, 0);
    glVertexArrayAttribBinding(globalVAO_, 1, 0);
    glVertexArrayAttribBinding(globalVAO_, 2, 0);
    glVertexArrayAttribBinding(globalVAO_, 3, 0);
}

void Renderer::createBufferMap(const std::vector<DrawEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
                               const SceneTotals &totals)
{
    std::vector<BufferMap> bufferMap;
    bufferMap.reserve(totals.visibleInstances);

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        std::uint32_t bucket{0};

        for (std::uint32_t j{0}; j < static_cast<std::uint32_t>(drawBuckets_.size() - 1); ++j)
        {
            if (i >= drawBuckets_[j].startIndex && i < drawBuckets_[j + 1].startIndex)
            {
                break;
            }
            else
            {
                ++bucket;
            }
        }

        const std::uint32_t numTransforms{static_cast<std::uint32_t>(entries[i].transforms->size())};
        const std::uint32_t matricesBufferBaseIndex{transformsByHandle_.at(entries[i].handle).matricesBufferBaseIndex};
        const std::uint32_t visibleInstancesBufferBaseIndex{metadata[i].visibleInstancesBufferBaseIndex};

        for (std::uint32_t k{0}; k < numTransforms; ++k)
        {
            bufferMap.emplace_back(BufferMap{
                .matricesIndex = matricesBufferBaseIndex + k,
                .sharedBaseIndex = i,
                .drawBucket = bucket,
                .drawBucketBaseIndex = drawBuckets_[bucket].startIndex,
                .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
            });
        }
    }

    glNamedBufferSubData(mapBuffer_, 0, bufferMap.size() * sizeof(BufferMap), bufferMap.data());
}

void Renderer::populateMatricesSSBO(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata,
                                    const SceneTotals &totals)
{
    std::uint32_t matricesIndex{0};
    std::vector<glm::mat4> modelMatrices;
    modelMatrices.reserve(totals.matrices);

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        auto [iter, inserted] = transformsByHandle_.try_emplace(entries[i].handle);

        if (inserted)
        {
            std::vector<Transform> &transforms{*entries[i].transforms};

            for (Transform &transform : transforms)
                modelMatrices.emplace_back(transform.getModelMatrix());

            iter->second = {matricesIndex, &transforms};

            matricesIndex += static_cast<std::uint32_t>(transforms.size());
        }
    }

    glNamedBufferSubData(matricesBuffer_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
}

void Renderer::updateMetadataCubemaps(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        using enum ShaderType;

        if (entries[i].shaderType == water || entries[i].shaderType == ocean)
        {
            const DrawEntry &drawEntry{entries[i]};
            PrimitiveMetadata &metaEntry{metadata[i]};

            std::span<const Material> materials{textureManager_.getMaterials(drawEntry.handle)};
            const int matIndex{drawEntry.primitive->materialIndex};
            const Material &material{materials[matIndex]};

            const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};

            metaEntry.cubemapArray = cubemap.array;
            metaEntry.cubemapLayer = cubemap.layer;

            glNamedBufferSubData(metadataBuffer_, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                                 static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
        }
    }
}