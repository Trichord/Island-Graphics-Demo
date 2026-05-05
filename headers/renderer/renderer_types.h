#pragma once

#include <cstdint>
#include <vector>

#include "cascade_shadow.h"

#pragma warning(push)
#pragma warning(disable : 5260)
#include "fastgltf/types.hpp"
#include "json/json.hpp"
#pragma warning(pop)

#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "primitive.h"
#include "texture_manager.h"
#include "transform.h"

// Struct for MultiDrawElements
struct DrawElementsIndirectCommand
{
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
};

// Describes a single gLTF primitive for the renderer
struct DrawEntry
{
    Handle handle;
    Primitive *primitive;
    std::vector<Transform> *transforms;

    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};

// Packs all uniforms for sharing across shaders
struct PackedShaderUniforms
{
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};
    glm::mat4 inverseViewProjectMatrix{1.0f};
    glm::vec4 cameraPosition{0.0f};
    float time{0.0f};
};

// Packed data for light transforms and frustum splits
struct PackedCascadeShadows
{
    glm::vec4 splits;
    glm::mat4 matrices[shadowCascadeCount];
};

// Textures and buffer indexes for bindless drawing
struct PrimitiveMetadata
{
    int albedoArray{0};
    int albedoLayer{0};
    int armArray{0};
    int armLayer{0};
    int normalArray{0};
    int normalLayer{0};
    int cubemapArray{0};
    int cubemapLayer{0};
    std::uint32_t visibleInstancesBufferBaseIndex{0};
};

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

// Buffer offset and primitive count for Multi Draw Indirect calls
struct MultiDrawBucket
{
    std::uint32_t maxCount{0};
    std::uint32_t startIndex{0};
    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};

struct BufferTransforms
{
    std::uint32_t matricesBufferBaseIndex{0};
    std::vector<Transform> *transforms{nullptr};
};

// Maps an entry in the visibility buffer, so that a compute shader global ID can find its resources
struct BufferMap
{
    std::uint32_t matricesIndex{0};
    std::uint32_t sharedBaseIndex{0};     
    std::uint32_t drawBucket{0};          
    std::uint32_t drawBucketBaseIndex{0};
    std::uint32_t visibleInstancesBaseIndex{0};
};

enum class BindingPointSSBO : unsigned int
{
    map,
    aabb,
    matrices,
    metadata,
    drawCounts,
    cullResults,
    drawCommands,
    visibleInstances,
    drawCommandsSource,
};

enum class BindingPointUBO : unsigned int
{
    shaderUniforms,
    cascadeShadows,
    frustumPlanes
};