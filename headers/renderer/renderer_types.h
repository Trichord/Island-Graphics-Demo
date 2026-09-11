#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>

#include "models/primitive.h"
#include "renderer/render_order.h"
#include "renderer/shader/graphics_shader.h"
#include "renderer/textures.h"
#include <glad/glad.h>

// Struct for MultiDrawElements
struct DrawElementsIndirectCommand
{
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
};

static_assert(sizeof(DrawElementsIndirectCommand) == 20);
static_assert(std::is_standard_layout_v<DrawElementsIndirectCommand>);

struct BucketState
{
    GLenum primitiveMode{GL_TRIANGLES};
    GLenum depthFunction{GL_LESS};
    RenderOrder renderOrder{RenderOrder::baseColor};
    AlphaMode alphaMode{AlphaMode::opaque};
    bool castsShadows{true};
    bool writeDepth{true};
};

constexpr BucketState createBucketState(ShaderType shader, AlphaMode alphaMode)
{
    using enum RenderOrder;
    using enum AlphaMode;

    BucketState state{
        .renderOrder = toRenderOrder(shader),
        .alphaMode = alphaMode,

        // Disable depth writing for blended objects, otherwise Hi-Z culling gets falsely triggered for objects
        // behind the blend
        .writeDepth = state.alphaMode != AlphaMode::blend,
    };

    switch (state.renderOrder)
    {
    case baseColor:
    case pbr:
    case reflectiveCubemap:
    case vegetation:
    case water:
        break;
    case particles:
        state.primitiveMode = GL_POINTS;
        break;
    case ocean:
        state.castsShadows = false;
        break;
    case skybox:
        state.depthFunction = GL_LEQUAL;
        state.castsShadows = false;
        state.writeDepth = false;
        break;
    }

    return state;
}

// Buffer offset and primitive count for Multi Draw Indirect calls
struct MultiDrawBucket
{
    std::uint32_t maxDrawCalls{0};
    std::uint32_t drawCommandsBaseIndex{0};
    BucketState state;
};

// GPU resident metadata for texture and buffer indexing
struct PrimitiveMetadata
{
    SamplerArrayIndex albedo;
    SamplerArrayIndex ARM;
    SamplerArrayIndex normal;
    SamplerArrayIndex cubemap;
    std::uint32_t visibleInstancesBaseIndex{0};
};

// Maps an entry in the visibility buffer, so that a compute shader global ID can find its resources
struct VisibilityBufferMap
{
    std::uint32_t sharedBufferIndex{0};
    std::uint32_t drawBucket{0};
    std::uint32_t drawCommandsBaseIndex{0};
    std::uint32_t visibleInstancesBaseIndex{0};
};

// Packs all uniforms for sharing across shaders
struct PackedCameraUniforms
{
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};
    glm::mat4 inverseViewProjectMatrix{1.0f};
    glm::vec4 cameraPosition{0.0f};
    float fov{45.0f};
};

struct PackedTime
{
    float deltaTime{0.0f};
    float timeElapsed{0.0f};
};

struct SceneUBO
{
    glm::vec4 lightDir{0.0f};
    glm::vec4 lightColor{1.0f};
    glm::vec4 ambientLight{1.0f};

    glm::vec4 windDirection{0.0f};
    float windStrength{0.0f};
};

struct WindowUBO
{
    int width{0};
    int height{0};
    float gamma{1.0f};
};