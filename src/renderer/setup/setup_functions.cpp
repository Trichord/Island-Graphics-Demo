#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

SceneTotals getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.transforms.size()};
        std::size_t numPrimitives{entry.primitives.size()};

        if (entry.shaderType == ShaderType::water)
            ++totals.dynamicCubemaps;

        totals.matrices += numTransforms;
        totals.primitives += numPrimitives;
        totals.visibleInstances += numTransforms * numPrimitives;

        for (Primitive &primitive : entry.primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();
        }
    }

    return totals;
}

// Build up all scene entries by shader type
std::vector<DrawEntry> createDrawEntries(Scene &scene, const SceneTotals &totals)
{
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(totals.primitives);

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        std::uint32_t numTransforms{static_cast<std::uint32_t>(entry.transforms.size())};

        for (Primitive &primitive : entry.primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries.emplace_back(DrawEntry{.handle = entry.handle,
                                               .primitive = &primitive,
                                               .transforms = &entry.transforms,
                                               .shaderType = entry.shaderType,
                                               .alphaMode = primitive.alphaMode});
        }
    }

    return drawEntries;
}

std::vector<MultiDrawBucket> createDrawBuckets(const std::vector<DrawEntry> &entries)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    std::vector<MultiDrawBucket> buckets;

    ShaderType currentShaderType{entries.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{entries.front().alphaMode};

    std::uint32_t start{0};
    std::uint32_t end{0};

    for (const DrawEntry &entry : entries)
    {
        if (currentShaderType != entry.shaderType || currentAlphaMode != entry.alphaMode)
        {
            buckets.emplace_back(MultiDrawBucket{.maxCount = end - start,
                                                 .startIndex = start,
                                                 .shaderType = currentShaderType,
                                                 .alphaMode = currentAlphaMode});

            start = end;
            currentShaderType = entry.shaderType;
            currentAlphaMode = entry.alphaMode;
        }

        ++end;
    }

    if (end - start > 0)
        buckets.emplace_back(MultiDrawBucket{.maxCount = end - start,
                                             .startIndex = start,
                                             .shaderType = currentShaderType,
                                             .alphaMode = currentAlphaMode});

    return buckets;
}

std::vector<DrawElementsIndirectCommand> createDrawCommandBuffer(std::vector<DrawEntry> &entries, SceneTotals &totals)
{
    std::vector<DrawElementsIndirectCommand> commands;
    commands.reserve(totals.primitives);

    int vaoIndex{0};
    std::uint32_t eboIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        DrawEntry &entry{entries[i]};
        const std::vector<Transform> &transforms{*entry.transforms};
        std::size_t numTransforms{transforms.size()};

        commands.emplace_back(
            DrawElementsIndirectCommand{.count = static_cast<unsigned int>(entry.primitive->indices.size()),
                                        .instanceCount = static_cast<unsigned int>(numTransforms),
                                        .firstIndex = eboIndex,
                                        .baseVertex = vaoIndex,
                                        .baseInstance = static_cast<unsigned int>(i)});

        vaoIndex += static_cast<std::uint32_t>(entry.primitive->vertices.size());
        eboIndex += static_cast<std::uint32_t>(entry.primitive->indices.size());
    }

    return commands;
}

std::vector<AABB> createAABBs(const std::vector<DrawEntry> &entries, const SceneTotals &totals)
{
    std::vector<AABB> aabbs;
    aabbs.reserve(totals.primitives);

    for (const DrawEntry &entry : entries)
    {
        const AABB &aabb{entry.primitive->aabb};
        aabbs.emplace_back(aabb);
    }

    return aabbs;
}

void setVisibleInstancesBaseIndex(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata)
{
    std::uint32_t visibleIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const std::vector<Transform> &transforms{*entries[i].transforms};
        const std::uint32_t numTransforms{static_cast<uint32_t>(transforms.size())};

        metadata[i].visibleInstancesBufferBaseIndex = visibleIndex;
        visibleIndex += numTransforms;
    }
}

void setMetadataMaterials(TextureManager &manager, const std::vector<DrawEntry> &entries,
                          std::vector<PrimitiveMetadata> &metadata)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        const DrawEntry &entry{entries[i]};
        PrimitiveMetadata &metaEntry{metadata[i]};

        const int matIndex{entry.primitive->materialIndex};
        std::span<const Material> materials{manager.getMaterials(entry.handle)};
        const Material &material{materials[matIndex]};

        if (matIndex < 0 || matIndex >= static_cast<int>(materials.size()))
        {
            std::cerr << "Material index out of bound for handle " << entry.handle << std::endl;
            throw std::runtime_error("Material index out of bounds");
        }

        const SamplerArrayIndex &albdeo{material.layers[Material::albedo]};
        const SamplerArrayIndex &ARM{material.layers[Material::ARM]};
        const SamplerArrayIndex &normal{material.layers[Material::normal]};
        const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};

        metaEntry.albedoArray = albdeo.array;
        metaEntry.albedoLayer = albdeo.layer;

        metaEntry.armArray = ARM.array;
        metaEntry.armLayer = ARM.layer;

        metaEntry.normalArray = normal.array;
        metaEntry.normalLayer = normal.layer;

        metaEntry.cubemapArray = cubemap.array;
        metaEntry.cubemapLayer = cubemap.layer;
    }
}

AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (SceneEntry &entry : scene.getAllSceneEntries())
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