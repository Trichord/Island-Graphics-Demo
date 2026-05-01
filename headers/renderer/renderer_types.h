#pragma once

#include <cstdint>
#include <vector>

#include "cascade_shadow.h"

#pragma warning(push)
#pragma warning(disable: 5260)
#include "fastgltf/types.hpp"
#include "json/json.hpp"
#pragma warning(pop)

#include "glm/glm.hpp"
#include "primitive.h"
#include "shader.h"
#include "texture_manager.h"
#include "transform.h"

// Struct for MultiDrawElements
typedef struct
{
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
} DrawElementsIndirectCommand;

// Describes a single gLTF primitive for the renderer
struct DrawEntry
{
    ModelHandle handle;
    Primitive *primitive;
    std::vector<Transform> *transforms;

    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};

    bool anyVisible{true};
    std::uint32_t visibleInstances{0};

    int vaoBaseIndex{0};
    std::uint32_t eboBaseIndex{0};
    std::uint32_t metadataBufferIndex{0};
    std::uint32_t matricesBufferBaseIndex{0};
    std::uint32_t visibleIndiciesBufferBaseIndex{0};
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
    std::uint32_t matricesBufferBaseIndex{0};
    std::uint32_t visibleInstancesBufferBaseIndex{0};
};

// Keep track of the number of vertices and indices in an entire scene
struct SceneTotals
{
    std::size_t vertices{0};
    std::size_t indices{0};
    std::size_t matrices{0};
    std::size_t visibleInstances{0};
};

// Buffer offset and primitive count for Multi Draw Indirect calls
struct MultiDrawInfo
{
    int count{0};
    int startIndex{0};
    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};