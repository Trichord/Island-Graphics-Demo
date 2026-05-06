#include <span>
#include <stdexcept>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "primitive.h"
#include "renderable.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
#include "texture_manager.h"
#include "transform.h"

AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        if (entry.shaderType == ocean || entry.shaderType == skybox)
            continue;

        Renderable &renderable{entry.renderableRef.get()};
        const AABB &aabb{renderable.getAABB()};

        for (Transform &transform : entry.transforms)
        {
            const AABB worldAABB{aabb.transform(transform.getModelMatrix())};

            const glm::vec3 aabbMin{worldAABB.getMin()};
            const glm::vec3 aabbMax{worldAABB.getMax()};

            min = glm::min(min, aabbMin);
            max = glm::max(max, aabbMax);
        }
    }

    return {min, max};
}

SceneTotals getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Renderable &renderable{entry.renderableRef.get()};
        std::optional<Handle> borrowed{renderable.getBorrowedHandle()};
        std::span<Primitive> primitives{renderable.getPrimitives()};

        std::size_t numTransforms{entry.transforms.size()};
        std::size_t numPrimitives{primitives.size()};

        if (entry.shaderType == ShaderType::water && !borrowed.has_value())
            ++totals.dynamicCubemaps;

        totals.matrices += numTransforms;
        totals.primitives += numPrimitives;
        totals.visibleInstances += numTransforms * numPrimitives;

        for (Primitive &primitive : primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();
        }
    }

    return totals;
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

// Build up all scene entries by shader type
std::vector<DrawEntry> createDrawEntries(Scene &scene, const SceneTotals &totals)
{
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(totals.primitives);

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Renderable &renderable{entry.renderableRef.get()};
        Handle handle{renderable.getHandle()};

        std::span<Primitive> primitives{renderable.getPrimitives()};
        std::uint32_t numTransforms{static_cast<std::uint32_t>(entry.transforms.size())};

        for (Primitive &primitive : primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries.emplace_back(DrawEntry{.handle = handle,
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