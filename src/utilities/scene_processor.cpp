#include "scene_processor.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aabb.h"
#include "buffer_builder.h"
#include "buffers.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

namespace
{

AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        if (entry.shaderType == ocean || entry.shaderType == skybox)
            continue;

        for (Transform &transform : entry.transforms)
        {
            const AABB worldAABB{entry.aabb.transform(transform.getModelMatrix())};

            const glm::vec3 aabbMin{worldAABB.getMin()};
            const glm::vec3 aabbMax{worldAABB.getMax()};

            min = glm::min(min, aabbMin);
            max = glm::max(max, aabbMax);
        }
    }

    return {min, max};
}

void processSceneError()
{
    std::cerr << "ERROR: SceneProcessor::processScene() has not been called" << std::endl;
    throw std::logic_error("SceneProcessor::processScene() has not been called");
}

void drawEntriesError()
{
    std::cerr << "ERROR: Draw entries empty on a non-empty scene" << std::endl;
    throw std::logic_error("Draw entries empty");
}

void metadataError()
{
    std::cerr << "ERROR: metadata empty on a non-empty scene" << std::endl;
    throw std::logic_error("metadata empty");
}

void drawCommandsError()
{
    std::cerr << "ERROR: Draw commands or buckets not built" << std::endl;
    throw std::logic_error("Draw commands or buckets empty");
}

} // namespace

SceneProcessor::SceneProcessor(Scene &scene, TextureManager &textureManager) : textureManager_{textureManager}
{
    if (scene.getAllSceneEntries().empty())
    {
        std::cerr << "ERROR: SceneProcessor called with empty scene" << std::endl;
        throw std::logic_error("SceneProcessor called with empty scene");
    }

    sceneAABB_ = getSceneBounds(scene);
    sceneTotals_ = getSceneTotals(scene);
    createDrawEntries(scene);
}

SceneProcessor::ProcessedScene SceneProcessor::processScene()
{
    if (!textureManager_.allTexturesBuilt())
    {
        std::cerr << "ERROR: SceneProcessor::processScene() called before TextureManager::buildAllTextures()"
                  << std::endl;
        throw std::logic_error("SceneProcessor::processScene() called before TextureManager::buildAllTextures()");
    }

    // Must be in order of draw entries (built from scene) -> commands -> buckets
    createDrawCommands();
    createDrawBuckets();

    // Everything below relies on draw entries, commands, and buckets.
    calculateBufferSizes();
    createAABBs();

    // Must be after draw commands and AABBs, since those are uploaded and immutable
    createSSBOHandles();

    // Metadata and buffer uploads. Must be after all SSBO handles are created
    createAndUploadMatrices();
    createAndUploadMetadata();

    // Relies on metadata and matrices
    createParticleInfo();
    createAndUploadBufferMap();
    createVAO();

    sceneProcessed_ = true;

    return {
        .ssbos = std::move(SSBOs_),
        .vao = std::move(vao_),
        .buckets = std::move(drawBuckets_),
        .particleInfo = std::move(particles_),
        .commands = std::move(drawCommands_),
        .matricesBaseIndexMap = std::move(matricesBaseIndex_),
        .worldAABB = std::move(sceneAABB_),
        .numInstances = sceneTotals_.instances,
        .numPrimitives = sceneTotals_.primitives,
    };
}

void SceneProcessor::calculateBufferSizes()
{
    if (drawCommands_.empty() || drawBuckets_.empty())
        drawCommandsError();

    bufferSizes_.metadata = drawCommands_.size() * sizeof(PrimitiveMetadata);
    bufferSizes_.matrices = sceneTotals_.matrices * sizeof(glm::mat4);
    bufferSizes_.bufferMap = sceneTotals_.instances * sizeof(BufferMap);
    bufferSizes_.drawCounts = drawBuckets_.size() * sizeof(std::uint32_t);
    bufferSizes_.cullResults = drawCommands_.size() * sizeof(std::uint32_t);
    bufferSizes_.visibleInstances = sceneTotals_.instances * sizeof(std::uint32_t);
}

std::size_t SceneProcessor::getNumDynamicCubemaps() const
{
    return sceneTotals_.dynamicCubemaps;
}

std::optional<Handle> SceneProcessor::getSkyboxHandle() const
{
    auto skyboxIter = std::ranges::lower_bound(drawEntries_, ShaderType::skybox, {}, &DrawEntry::shaderType);

    if (skyboxIter == drawEntries_.end())
        return {};
    else
        return skyboxIter->handle;
}

std::optional<SceneProcessor::ShaderBucketInfo> SceneProcessor::getShaderBucketInfo(ShaderType shader) const
{
    if (!sceneProcessed_)
        processSceneError();

    ShaderBucketInfo info;

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        if (drawEntries_[i].shaderType == shader)
        {
            info.drawCommandsStartIndex = i;

            while (i < drawEntries_.size() && drawEntries_[i].shaderType == shader)
            {
                info.handles.push_back(drawEntries_[i++].handle);
                ++info.size;
            }

            return info;
        }
    }

    return {};
}

SceneProcessor::SceneTotals SceneProcessor::getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.transforms.size()};
        std::size_t numPrimitives{entry.primitives.size()};

        if (entry.shaderType == ShaderType::water)
            ++totals.dynamicCubemaps;

        totals.matrices += numTransforms;
        totals.primitives += numPrimitives;
        totals.instances += numTransforms * numPrimitives;

        for (Primitive &primitive : entry.primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();
        }
    }

    return totals;
}

// Build up all scene entries by shader type
void SceneProcessor::createDrawEntries(Scene &scene)
{
    drawEntries_.reserve(sceneTotals_.primitives);

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::uint32_t numTransforms{static_cast<std::uint32_t>(entry.transforms.size())};

        for (Primitive &primitive : entry.primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries_.emplace_back(DrawEntry{.handle = entry.handle,
                                                .primitive = &primitive,
                                                .transforms = entry.transforms,
                                                .shaderType = entry.shaderType,
                                                .alphaMode = primitive.alphaMode});
        }
    }

    std::ranges::sort(drawEntries_, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });
}

void SceneProcessor::createDrawCommands()
{
    if (drawEntries_.empty())
        drawEntriesError();

    drawCommands_.reserve(sceneTotals_.primitives);

    std::size_t vaoIndex{0};
    std::size_t eboIndex{0};

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        DrawEntry &entry{drawEntries_[i]};
        Primitive &primitive{*entry.primitive};
        std::size_t numIndices{primitive.indices.size()};

        drawCommands_.emplace_back(
            DrawElementsIndirectCommand{.count = static_cast<unsigned int>(numIndices),
                                        .instanceCount = static_cast<unsigned int>(entry.transforms.size()),
                                        .firstIndex = static_cast<unsigned int>(eboIndex),
                                        .baseVertex = static_cast<int>(vaoIndex),
                                        .baseInstance = static_cast<unsigned int>(i)});

        vaoIndex += primitive.vertices.size();
        eboIndex += numIndices;
    }
}

void SceneProcessor::createAABBs()
{
    AABBs_.reserve(sceneTotals_.primitives);

    for (const DrawEntry &entry : drawEntries_)
    {
        const AABB &aabb{entry.primitive->aabb};
        AABBs_.emplace_back(aabb);
    }
}

void SceneProcessor::createDrawBuckets()
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    if (drawEntries_.empty())
        drawEntriesError();

    ShaderType currentShaderType{drawEntries_.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{drawEntries_.front().alphaMode};

    std::uint32_t start{0};
    std::uint32_t end{0};

    for (const DrawEntry &entry : drawEntries_)
    {
        if (currentShaderType != entry.shaderType || currentAlphaMode != entry.alphaMode)
        {
            drawBuckets_.emplace_back(MultiDrawBucket{.maxDrawCalls = end - start,
                                                      .drawCommandsBaseIndex = start,
                                                      .shaderType = currentShaderType,
                                                      .alphaMode = currentAlphaMode});

            start = end;
            currentShaderType = entry.shaderType;
            currentAlphaMode = entry.alphaMode;
        }

        ++end;
    }

    if (end - start > 0)
        drawBuckets_.emplace_back(MultiDrawBucket{
            .maxDrawCalls = end - start,
            .drawCommandsBaseIndex = start,
            .shaderType = currentShaderType,
            .alphaMode = currentAlphaMode,
        });
}

void SceneProcessor::createAndUploadMatrices()
{
    if (!buffersBuilt_ || !SSBOs_.matrices)
    {
        std::cerr << "ERROR: Matrices buffer not built" << std::endl;
        throw std::logic_error("Matrices buffer not built");
    }

    if (drawEntries_.empty())
        drawEntriesError();

    std::uint32_t matricesIndex{0};
    matrices_.reserve(sceneTotals_.matrices);

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        auto [iter, inserted] = matricesBaseIndex_.try_emplace(drawEntries_[i].handle);

        if (inserted)
        {
            for (Transform &transform : drawEntries_[i].transforms)
                matrices_.emplace_back(transform.getModelMatrix());

            iter->second = matricesIndex;
            matricesIndex += static_cast<std::uint32_t>(drawEntries_[i].transforms.size());
        }
    }
    glNamedBufferSubData(SSBOs_.matrices, 0, static_cast<GLsizeiptr>(matrices_.size() * sizeof(glm::mat4)),
                         matrices_.data());
}

void SceneProcessor::createAndUploadMetadata()
{
    if (!buffersBuilt_ || !SSBOs_.metadata)
    {
        std::cerr << "ERROR: Metadata buffer not built" << std::endl;
        throw std::logic_error("Metadata buffer not built");
    }

    metadata_.resize(drawEntries_.size());
    setVisibleInstancesBaseIndex();
    setMetadataMaterials();

    glNamedBufferSubData(SSBOs_.metadata, 0, static_cast<GLsizeiptr>(metadata_.size() * sizeof(PrimitiveMetadata)),
                         metadata_.data());
}

void SceneProcessor::setVisibleInstancesBaseIndex()
{
    if (drawEntries_.empty())
        drawEntriesError();

    std::uint32_t visibleIndex{0};

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        const std::size_t numTransforms{drawEntries_[i].transforms.size()};

        metadata_[i].visibleInstancesBaseIndex = visibleIndex;
        visibleIndex += static_cast<std::uint32_t>(numTransforms);
    }
}

void SceneProcessor::setMetadataMaterials()
{
    if (drawEntries_.empty())
        drawEntriesError();

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        const DrawEntry &entry{drawEntries_[i]};
        PrimitiveMetadata &metaEntry{metadata_[i]};

        const int matIndex{entry.primitive->materialIndex};
        std::span<const Material> materials{textureManager_.getMaterials(entry.handle)};

        if (matIndex < 0 || matIndex >= static_cast<int>(materials.size()))
        {
            std::cerr << "Material index out of bound for handle " << entry.handle << std::endl;
            throw std::runtime_error("Material index out of bounds");
        }

        const Material &material{materials[matIndex]};
        const SamplerArrayIndex &albedo{material.layers[Material::albedo]};
        const SamplerArrayIndex &ARM{material.layers[Material::ARM]};
        const SamplerArrayIndex &normal{material.layers[Material::normal]};
        const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};

        metaEntry.albedoArray = albedo.array;
        metaEntry.albedoLayer = albedo.layer;

        metaEntry.armArray = ARM.array;
        metaEntry.armLayer = ARM.layer;

        metaEntry.normalArray = normal.array;
        metaEntry.normalLayer = normal.layer;

        metaEntry.cubemapArray = cubemap.array;
        metaEntry.cubemapLayer = cubemap.layer;
    }
}

void SceneProcessor::createAndUploadBufferMap()
{
    if (drawEntries_.empty())
        drawEntriesError();

    if (metadata_.empty())
        metadataError();

    if (drawCommands_.empty() || drawBuckets_.empty())
        drawCommandsError();

    bufferMap_.reserve(sceneTotals_.instances);

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        MultiDrawBucket &bucket{drawBuckets_[i]};

        for (std::uint32_t j{bucket.drawCommandsBaseIndex}; j < bucket.drawCommandsBaseIndex + bucket.maxDrawCalls; ++j)
        {
            const Handle handle{drawEntries_[j].handle};
            const PrimitiveMetadata &metadata{metadata_[j]};
            const std::span<Transform> transforms{drawEntries_[j].transforms};

            const std::uint32_t matricesBufferBaseIndex{matricesBaseIndex_.at(handle)};
            const std::uint32_t visibleInstancesBufferBaseIndex{metadata.visibleInstancesBaseIndex};
            const std::uint32_t currentBucket{static_cast<std::uint32_t>(i)};

            for (std::size_t k{0}; k < transforms.size(); ++k)
            {
                bufferMap_.emplace_back(BufferMap{
                    .matricesIndex = matricesBufferBaseIndex + static_cast<std::uint32_t>(k),
                    .sharedBaseIndex = j,
                    .drawBucket = currentBucket,
                    .drawCommandsBaseIndex = bucket.drawCommandsBaseIndex,
                    .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
                });
            }
        }
    }

    glNamedBufferSubData(SSBOs_.map, 0, static_cast<GLsizeiptr>(bufferMap_.size() * sizeof(BufferMap)),
                         bufferMap_.data());
}

void SceneProcessor::createVAO()
{
    if (drawEntries_.empty())
        drawEntriesError();

    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(sceneTotals_.vertices);
    indices.reserve(sceneTotals_.indices);

    for (const DrawEntry &entry : drawEntries_)
    {
        std::copy(entry.primitive->vertices.begin(), entry.primitive->vertices.end(), std::back_inserter(vertices));
        std::copy(entry.primitive->indices.begin(), entry.primitive->indices.end(), std::back_inserter(indices));
    }

    BufferBuilder builder;
    vao_ = builder.buildVAO(vertices, indices);
}

void SceneProcessor::createSSBOHandles()
{
    if (drawCommands_.empty())
        drawCommandsError();

    if (AABBs_.empty())
    {
        std::cerr << "ERROR: AABBs not built" << std::endl;
        throw std::logic_error("AABBs not built");
    }

    BufferBuilder builder;
    SSBOs_ = builder.buildSSBOs(drawCommands_, AABBs_, bufferSizes_);
    buffersBuilt_ = true;
}

void SceneProcessor::updateMetadataCubemaps(GLuint metadataBufferID)
{
    if (!sceneProcessed_)
        processSceneError();

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(drawEntries_.size()); ++i)
    {
        using enum ShaderType;

        if (drawEntries_[i].shaderType == water || drawEntries_[i].shaderType == ocean)
        {
            PrimitiveMetadata &metaEntry{metadata_[i]};

            const Handle handle{drawEntries_[i].handle};
            const Primitive &primitive{*drawEntries_[i].primitive};
            std::span<const Material> materials{textureManager_.getMaterials(handle)};
            const Material &material{materials[primitive.materialIndex]};

            const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};
            metaEntry.cubemapArray = cubemap.array;
            metaEntry.cubemapLayer = cubemap.layer;

            glNamedBufferSubData(metadataBufferID, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                                 static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
        }
    }
}

void SceneProcessor::createParticleInfo()
{
    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        if (drawEntries_[i].shaderType != ShaderType::particles)
            continue;

        std::uint32_t mapIndex{metadata_[i].visibleInstancesBaseIndex};
        std::uint32_t sharedBaseIndex{static_cast<std::uint32_t>(i)};

        particles_.emplace_back(ParticleCompute{
            .mapIndex = mapIndex,
            .sharedBufferIndex = sharedBaseIndex,
            .numParticles = drawEntries_[i].primitive->vertices.size(),
        });
    }
}
