#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#pragma warning(push)
#pragma warning(disable : 5260)
#include <fastgltf/types.hpp>
#include <nlohmann/json.hpp>
#pragma warning(pop)

#include "renderer/cascade_shadow.h"
#include "renderer/shader/graphics_shader.h"

// Struct for MultiDrawElements
struct DrawElementsIndirectCommand
{
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
};

// Buffer offset and primitive count for Multi Draw Indirect calls
struct MultiDrawBucket
{
    std::uint32_t maxDrawCalls{0};
    std::uint32_t drawCommandsBaseIndex{0};
    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};

// GPU resident metadata for texture and buffer indexing
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
    std::uint32_t visibleInstancesBaseIndex{0};
};

// Maps an entry in the visibility buffer, so that a compute shader global ID can find its resources
struct VisibilityBufferMap
{
    std::uint32_t matricesIndex{0};
    std::uint32_t sharedIndex{0};
    std::uint32_t drawBucket{0};
    std::uint32_t drawCommandsBaseIndex{0};
    std::uint32_t visibleInstancesBaseIndex{0};
};

// Maps a particle back to the VBO and shared index.
struct ParticleBufferMap
{
    std::uint32_t vboIndex{0};
    std::uint32_t sharedIndex{0};
};

// Packs all uniforms for sharing across shaders
struct PackedCameraUniforms
{
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};
    glm::mat4 inverseViewProjectMatrix{1.0f};
    glm::vec4 cameraPosition{0.0f};
};

// Packed data for light transforms and frustum splits
struct PackedCascadeShadows
{
    glm::vec4 near{0.0f};
    glm::vec4 far{0.0f};
    glm::mat4 matrices[shadowCascadeCount];
};

struct PackedTime
{
    float deltaTime{0.0f};
    float timeElapsed{0.0f};
};
