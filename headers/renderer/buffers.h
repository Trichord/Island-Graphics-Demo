#pragma once

#include <glad/glad.h>
#include <iterator>
#include <utility>

/* Shader Storage Buffer Objects
 *
 * Visibility Map: Per (primitive × instance) pair. Allows any index in the VisibleInstances buffer to see its matching:
 *      - sharedBaseIndex: index into AABBs/Metadata/Draw Commands/Model Matrix (the primitive)
 *      - drawBucket: which draw bucket this primitive belongs to
 *      - drawCommandsBaseIndex: start of that bucket in DrawCommands
 *      - visibleInstancesBaseIndex: start of the (primitive x instance) in the visibility buffer
 *
 * AABB: Per primitive. Local-space bounding boxes
 *
 * Matrices: Per primitive. World-space transforms
 *
 * Metadata: Per primitive. Material textures, visibility buffer base index
 *
 * Draw Counts: Per draw bucket. Number of surviving draw commands after compaction
 *
 * Cull Results: Per primitive. Number of surviving instances after frustum test
 *
 * Draw Commands: Per primitive. Compacted each frame from DrawCommandsSource
 *
 * Visible Instances: Per (primitive × instance) pair. Indices into Matrices buffer for
 *                   instances that passed the frustum test. Each primitive owns a
 *                   contiguous section sized to its instance count.
 *
 * Draw Commands Source: Per primitive. Immutable source of truth, set once at load
 *
 * Vertex Map: Per vertex (used by particles only). Allows any index in map buffer to look up its vertex in the VBO, and
 * its sharedBaseIndex.
 *
 * Bone Transforms: Per skinned model. Each model gets a dedicated sectionl, allowing for skeletal animation.
 *
 * Vertex Bone Data: Per skinned vertex. Each skinned vertex gets a direct index into this buffer. Golds weights and
 * bone IDs, keeping the vertex struct at 32 bytes.
 *
 * 1:1 indexed buffers: Draw Commands Source[i], AABB[i], Metadata[i]
 * Matrices[j], Visibility Map[j], Visible Instances[j] where j spans all (primitive × instance) pairs
 *
 * Buffers resized for cascade shadows: Draw Commands, Draw Counts, Cull Results, Visible Instances. Allows the main
 * render pass to own one section of the buffer, and each cascade owns its own section. Total size is sizeof(original
 * buffer) * (cascade count + 1)
 *
 */

// Holds the OpenGL handles for all Shader Storage Buffer Objects
struct SSBOs
{
    GLuint visibilityMap{0};
    GLuint aabb{0};
    GLuint matrices{0};
    GLuint metadata{0};
    GLuint drawCounts{0};
    GLuint cullResults{0};
    GLuint drawCommands{0};
    GLuint visibleInstances{0};
    GLuint drawCommandsSource{0};
    GLuint vertexMap{0};
    GLuint boneTransforms{0};
    GLuint vertexBoneData{0};

    SSBOs() = default;
    SSBOs(const SSBOs &other) = delete;
    SSBOs &operator=(const SSBOs &other) = delete;

    SSBOs(SSBOs &&other) noexcept
        : visibilityMap{std::exchange(other.visibilityMap, 0)}, aabb{std::exchange(other.aabb, 0)},
          matrices{std::exchange(other.matrices, 0)}, metadata{std::exchange(other.metadata, 0)},
          drawCounts{std::exchange(other.drawCounts, 0)}, cullResults{std::exchange(other.cullResults, 0)},
          drawCommands{std::exchange(other.drawCommands, 0)},
          visibleInstances{std::exchange(other.visibleInstances, 0)},
          drawCommandsSource{std::exchange(other.drawCommandsSource, 0)}, vertexMap{std::exchange(other.vertexMap, 0)},
          boneTransforms{std::exchange(other.boneTransforms, 0)}, vertexBoneData{std::exchange(other.vertexBoneData, 0)}
    {
    }

    SSBOs &operator=(SSBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{visibilityMap,      aabb,        matrices,       metadata,
                             drawCounts,         cullResults, drawCommands,   visibleInstances,
                             drawCommandsSource, vertexMap,   boneTransforms, vertexBoneData};
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            visibilityMap = std::exchange(other.visibilityMap, 0);
            aabb = std::exchange(other.aabb, 0);
            matrices = std::exchange(other.matrices, 0);
            metadata = std::exchange(other.metadata, 0);
            drawCounts = std::exchange(other.drawCounts, 0);
            cullResults = std::exchange(other.cullResults, 0);
            drawCommands = std::exchange(other.drawCommands, 0);
            visibleInstances = std::exchange(other.visibleInstances, 0);
            drawCommandsSource = std::exchange(other.drawCommandsSource, 0);
            vertexMap = std::exchange(other.vertexMap, 0);
            boneTransforms = std::exchange(other.boneTransforms, 0);
            vertexBoneData = std::exchange(other.vertexBoneData, 0);
        }

        return *this;
    }

    ~SSBOs()
    {
        GLuint buffers[]{visibilityMap,      aabb,        matrices,       metadata,
                         drawCounts,         cullResults, drawCommands,   visibleInstances,
                         drawCommandsSource, vertexMap,   boneTransforms, vertexBoneData};
        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

/*
 * Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 *
 * Shadow attribute layout:
 * 0 - position (vec3)
 * 1 - texture coordinates (vec2)
 */

struct VAOs
{
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLuint shadowVAO{0};
    GLuint shadowVBO{0};

    VAOs() = default;
    VAOs(const VAOs &other) = delete;
    VAOs &operator=(const VAOs &other) = delete;

    VAOs(VAOs &&other) noexcept
        : vao{std::exchange(other.vao, 0)}, vbo{std::exchange(other.vbo, 0)}, ebo{std::exchange(other.ebo, 0)},
          shadowVAO{std::exchange(other.shadowVAO, 0)}, shadowVBO{std::exchange(other.shadowVBO, 0)}
    {
    }

    VAOs &operator=(VAOs &&other) noexcept
    {
        if (this != &other)
        {
            glDeleteBuffers(1, &ebo);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &shadowVBO);
            glDeleteVertexArrays(1, &vao);
            glDeleteVertexArrays(1, &shadowVAO);

            vao = std::exchange(other.vao, 0);
            vbo = std::exchange(other.vbo, 0);
            ebo = std::exchange(other.ebo, 0);
            shadowVAO = std::exchange(other.shadowVAO, 0);
            shadowVBO = std::exchange(other.shadowVBO, 0);
        }

        return *this;
    }

    ~VAOs()
    {
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &shadowVBO);
        glDeleteVertexArrays(1, &vao);
        glDeleteVertexArrays(1, &shadowVAO);
    }
};

struct UBOs
{
    GLuint camera{0};
    GLuint frustumPlanes{0};
    GLuint cascadeShadows{0};
    GLuint time{0};

    UBOs() = default;
    UBOs(const UBOs &other) = delete;
    UBOs &operator=(const UBOs &other) = delete;

    UBOs(UBOs &&other) noexcept
        : camera{std::exchange(other.camera, 0)}, frustumPlanes{std::exchange(other.frustumPlanes, 0)},
          cascadeShadows{std::exchange(other.cascadeShadows, 0)}, time{std::exchange(other.time, 0)}
    {
    }

    UBOs &operator=(UBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{camera, frustumPlanes, cascadeShadows, time};
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            camera = std::exchange(other.camera, 0);
            frustumPlanes = std::exchange(other.frustumPlanes, 0);
            cascadeShadows = std::exchange(other.cascadeShadows, 0);
            time = std::exchange(other.time, 0);
        }

        return *this;
    }

    ~UBOs()
    {
        GLuint buffers[]{camera, frustumPlanes, cascadeShadows, time};
        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

// Uniform Buffer Object binding points (unsigned int)
enum class BindingPointUBO : unsigned int
{
    camera,
    frustumPlanes,
    cascadeShadows,
    time
};

// Shader Storage Buffer Object binding points (unsigned int). Must be <= 16 total
enum class BindingPointSSBO : unsigned int
{
    visibilityMap,
    aabb,
    matrices,
    metadata,
    drawCounts,
    cullResults,
    drawCommands,
    visibleInstances,
    drawCommandsSource,
    vbo,
    vertexMap,
    shadowDrawCommands,
    shadowCullResults,
    shadowDrawCounts,
    boneTransforms,
    vertexBoneData
};

namespace SSBOMinimum
{
inline constexpr int combinedBlocks{16};
inline constexpr int combinedBindings{16};
inline constexpr int vertexBlocks{4};
inline constexpr int fragmentBlocks{4};
} // namespace SSBOMinimum