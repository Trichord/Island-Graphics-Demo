#pragma once

#include <cstdint>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 5260)
#include "fastgltf/types.hpp"
#include "json/json.hpp"
#pragma warning(pop)

#include "cascade_shadow.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
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

// Packs all uniforms for sharing across shaders
struct PackedCameraUniforms
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

// Buffer offset and primitive count for Multi Draw Indirect calls
struct MultiDrawBucket
{
    std::uint32_t maxDrawCalls{0};
    std::uint32_t drawCommandsBaseIndex{0};
    ShaderType shaderType{ShaderType::directionalPBR};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};

struct ParticleUniforms
{
    std::uint32_t sharedIndex{0};
    std::uint32_t localGroupSize{0};
};