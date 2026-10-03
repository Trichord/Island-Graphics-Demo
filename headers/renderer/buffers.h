#pragma once

#include <glad/glad.h>
#include <iterator>
#include <utility>

#include "renderer/cascade_shadow.h"

/* Shader Storage Buffer Objects
 * -----------------------------
 * 
 * ******************************************
 * Multi Draw Elements Indirect Count Buffers
 * ******************************************
 *
 * Draw Counts: Per draw bucket. Number of surviving draw commands after compaction.
 *
 * AABB: Per primitive. Local-space bounding boxes.
 *
 * Draw Commands: Per primitive. Compacted each frame from DrawCommandsSource.
 *
 * Draw Commands Source: Per primitive. Immutable source of truth, set once at load.
 *
 * Cull Results: Per primitive. Number of surviving instances after frustum test.
 *
 * Metadata: Per primitive. Material textures, visibility buffer base index.
 *
 * Matrices: Per (primitive × instance) pair. World-space transforms.
 *
 * Visible Instances: Per (primitive × instance) pair. Indices into Matrices buffer for
 *                   instances that passed the culling test(s). Each primitive owns a
 *                   contiguous section sized to its instance count.
 *
 * Visibility Map: Per (primitive × instance) pair. Allows any index in the VisibleInstances buffer to see its matching:
 *      - sharedIndex: index into AABBs/Metadata/Draw Commands/Model Matrix buffers (the primitive)
 *      - drawBucket: which draw bucket this primitive belongs to
 *      - drawCommandsBaseIndex: start of that bucket in DrawCommands
 *      - visibleInstancesBaseIndex: start of the (primitive x instance) range in the visibility buffer
 *
 ********************
 * Supporting Buffers
 ********************
 *
 * Bone Transforms: Per skinned model. Each model gets a dedicated section, allowing for skeletal animation.
 *
 * Vertex Bone Data: Per skinned vertex. Each skinned vertex gets its Bone IDs remapped as a direct index into this
 * buffer. Holds weights and bone IDs, keeping the vertex struct at 32 bytes.
 *
 * Skinned Vertex Source: Per skinned vertex. A copy of the original skinned vertex. Immutable source of truth for
 * compute shader vertex skinning, set once at load time.
 *
 * Particle Map: Per particle vertex. Allows any index in map buffer to look up its shared index (aabb, metadata, etc).
 *
 * Exploding Vertices Map: Per exploded vertex (non-instanced). Allows any model which subscribes to the exploding effect to look up
 * its original VBO index, world space transform index, and metadata for driving the effect.
 *
 **********************
 * Implementation Notes
 **********************
 *
 * 1:1 indexed buffers: Draw Commands Source[i], AABB[i], Metadata[i]
 * Matrices[j], Visibility Map[j], Visible Instances[j] where j spans all (primitive × instance) pairs
 * Vertex Bone Data[k], Skinned Vertex Source[k]
 *
 * Buffers resized for cascade shadows: Draw Commands, Draw Counts, Cull Results, Visible Instances. Allows the main
 * render pass to own one section of the buffer, and each cascade owns its own section. Total size is sizeof(original
 * buffer) * (cascade count + 1).
 *
 */

// Holds the OpenGL handles for all Shader Storage Buffer Objects
struct SSBOs
{
    GLuint aabb{0};
    GLuint metadata{0};
    GLuint matrices{0};
    GLuint drawCounts{0};
    GLuint cullResults{0};
    GLuint drawCommands{0};
    GLuint visibilityMap{0};
    GLuint visibleInstances{0};
    GLuint drawCommandsSource{0};

    GLuint particleMap{0};
    GLuint boneTransforms{0};
    GLuint vertexBoneData{0};
    GLuint explodingVertexMap{0};
    GLuint skinnedVertexSource{0};

    SSBOs() = default;
    SSBOs(const SSBOs &other) = delete;
    SSBOs &operator=(const SSBOs &other) = delete;

    SSBOs(SSBOs &&other) noexcept
        : aabb{std::exchange(other.aabb, 0)}, metadata{std::exchange(other.metadata, 0)},
          matrices{std::exchange(other.matrices, 0)}, drawCounts{std::exchange(other.drawCounts, 0)},
          cullResults{std::exchange(other.cullResults, 0)}, drawCommands{std::exchange(other.drawCommands, 0)},
          visibilityMap{std::exchange(other.visibilityMap, 0)},
          visibleInstances{std::exchange(other.visibleInstances, 0)},
          drawCommandsSource{std::exchange(other.drawCommandsSource, 0)},
          particleMap{std::exchange(other.particleMap, 0)}, boneTransforms{std::exchange(other.boneTransforms, 0)},
          vertexBoneData{std::exchange(other.vertexBoneData, 0)},
          explodingVertexMap{std::exchange(other.explodingVertexMap, 0)},
          skinnedVertexSource{std::exchange(other.skinnedVertexSource, 0)}
    {
    }

    SSBOs &operator=(SSBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{
                aabb,           metadata,       matrices,           drawCounts,          cullResults,
                drawCommands,   visibilityMap,  visibleInstances,   drawCommandsSource,  particleMap,
                boneTransforms, vertexBoneData, explodingVertexMap, skinnedVertexSource,
            };
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            aabb = std::exchange(other.aabb, 0);
            metadata = std::exchange(other.metadata, 0);
            matrices = std::exchange(other.matrices, 0);
            drawCounts = std::exchange(other.drawCounts, 0);
            cullResults = std::exchange(other.cullResults, 0);
            drawCommands = std::exchange(other.drawCommands, 0);
            visibilityMap = std::exchange(other.visibilityMap, 0);
            visibleInstances = std::exchange(other.visibleInstances, 0);
            drawCommandsSource = std::exchange(other.drawCommandsSource, 0);

            particleMap = std::exchange(other.particleMap, 0);
            boneTransforms = std::exchange(other.boneTransforms, 0);
            vertexBoneData = std::exchange(other.vertexBoneData, 0);
            explodingVertexMap = std::exchange(other.explodingVertexMap, 0);
            skinnedVertexSource = std::exchange(other.skinnedVertexSource, 0);
        }

        return *this;
    }

    ~SSBOs()
    {
        GLuint buffers[]{
            aabb,           metadata,       matrices,           drawCounts,          cullResults,
            drawCommands,   visibilityMap,  visibleInstances,   drawCommandsSource,  particleMap,
            boneTransforms, vertexBoneData, explodingVertexMap, skinnedVertexSource,
        };

        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

/*
 * Vertex attribute layout:
 * 0 - Position (vec3)
 * 1 - Normal (uint / vec3)
 * 2 - Tangent (uint / vec4)
 * 3 - Texture coordinates (uint / vec2)
 * 4 - VBO Index (uint)
 * 5 - Color (uint / vec4)
 *
 * Shadow attribute layout:
 * 0 - position (vec3)
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
    GLuint scene{0};
    GLuint window{0};

    UBOs() = default;
    UBOs(const UBOs &other) = delete;
    UBOs &operator=(const UBOs &other) = delete;

    UBOs(UBOs &&other) noexcept
        : camera{std::exchange(other.camera, 0)}, frustumPlanes{std::exchange(other.frustumPlanes, 0)},
          cascadeShadows{std::exchange(other.cascadeShadows, 0)}, time{std::exchange(other.time, 0)},
          scene{std::exchange(other.scene, 0)}, window{std::exchange(other.window, 0)}
    {
    }

    UBOs &operator=(UBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{camera, frustumPlanes, cascadeShadows, time, scene, window};
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            camera = std::exchange(other.camera, 0);
            frustumPlanes = std::exchange(other.frustumPlanes, 0);
            cascadeShadows = std::exchange(other.cascadeShadows, 0);
            time = std::exchange(other.time, 0);
            scene = std::exchange(other.scene, 0);
            window = std::exchange(other.window, 0);
        }

        return *this;
    }

    ~UBOs()
    {
        GLuint buffers[]{camera, frustumPlanes, cascadeShadows, time, scene, window};
        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

// Uniform Buffer Object binding points (std::uint32_t)
enum class BindingPointUBO : std::uint32_t
{
    time,
    scene,
    camera,
    window,
    frustumPlanes,
    cascadeShadows,
};

// Shader Storage Buffer Object binding points (std::uint32_). Total number of buffers bound at any time must be <= 16
enum class BindingPointSSBO : std::uint32_t
{
    // Aliased VBOs
    vbo,
    positionVBO,

    // MDIC Buffers
    aabb,
    matrices,
    metadata,
    drawCounts,
    cullResults,
    drawCommands,
    visibilityMap,
    visibleInstances,
    drawCommandsSource,

    // Supporting buffers
    particleMap,
    boneTransforms,
    vertexBoneData,
    explodingVertexMap,
    skinnedVertexSource,
};

namespace SSBOMinimum
{
inline constexpr int combinedBlocks{16};
inline constexpr int combinedBindings{16};
inline constexpr int vertexBlocks{8};
inline constexpr int fragmentBlocks{8};
} // namespace SSBOMinimum

enum class BufferRange : std::size_t
{
    mainPass,
    cascade0,
    cascade1,
    cascade2,
    count
};

static_assert(static_cast<std::size_t>(BufferRange::cascade0) + CascadeShadow::cascadeCount ==
              static_cast<std::size_t>(BufferRange::count));

constexpr std::size_t bufferRangeIndex(BufferRange range)
{
    return static_cast<std::size_t>(range);
}

constexpr BufferRange bufferRangeFromIndex(std::size_t index)
{
    assert(index < static_cast<std::size_t>(BufferRange::count));
    return static_cast<BufferRange>(index);
}

constexpr BufferRange &operator++(BufferRange &r)
{
    const std::size_t n{static_cast<std::size_t>(r) + 1};
    assert(n < static_cast<std::size_t>(BufferRange::count));
    r = static_cast<BufferRange>(n);
    return r;
}

constexpr BufferRange operator+(BufferRange r, std::size_t i)
{
    const std::size_t n{static_cast<std::size_t>(r) + i};
    assert(n <= static_cast<std::size_t>(BufferRange::count));
    r = static_cast<BufferRange>(n);
    return r;
}

constexpr BufferRange operator+(std::size_t i, BufferRange r)
{
    return r + i;
}