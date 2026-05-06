#pragma once

#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "graphics_shader.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "transform.h"

// Keep track of the number of vertices and indices in an entire scene
struct SceneTotals
{
    std::size_t vertices{0};
    std::size_t indices{0};
    std::size_t matrices{0};
    std::size_t primitives{0};
    std::size_t dynamicCubemaps{0};
    std::size_t visibleInstances{0};
};

// Describes a single gLTF primitive for the renderer. Temporary struct for building Draw Commands
struct DrawEntry
{
    Handle handle;
    Primitive *primitive;
    std::vector<Transform> *transforms;

    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};

// Adding up scene properties
AABB getSceneBounds(Scene &scene);
SceneTotals getSceneTotals(Scene &scene);

// Creating resources for buffers
std::vector<DrawEntry> createDrawEntries(Scene &scene, const SceneTotals &totals);
std::vector<MultiDrawBucket> createDrawBuckets(const std::vector<DrawEntry> &entries);
std::vector<AABB> createAABBs(const std::vector<DrawEntry> &entries, const SceneTotals &totals);
std::vector<DrawElementsIndirectCommand> createDrawCommandBuffer(std::vector<DrawEntry> &entries, SceneTotals &totals);

// Updating resources
void setVisibleInstancesBaseIndex(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);
void setMetadataMaterials(TextureManager &manager, const std::vector<DrawEntry> &entries,
                          std::vector<PrimitiveMetadata> &metadata);
