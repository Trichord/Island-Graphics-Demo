#pragma once

#include <iterator>
#include <utility>

#include "glad/glad.h"

/* Buffers
 *
 * Overall strategy: maintain set regions for each primitive so that a base index is always known. From there, any
 * instancing is always base + instance.
 *
 * Map: Per (primitive × instance) pair. Allows any index in the VisibleInstances buffer to see its matching:
 *      - matricesIndex: index into Matrices buffer
 *      - sharedBaseIndex: index into AABBs/Metadata/DrawCommands (the primitive)
 *      - drawBucket: which draw bucket this primitive belongs to
 *      - drawBucketBaseIndex: start of that bucket in DrawCommands
 *      - visibleInstancesBaseIndex: start of the (primitive x instance) in the visibility buffer
 *
 * AABB: Per primitive. Local-space bounding boxes
 * Matrices: Per model. World-space transforms (multiple primitives share one range)
 * Metadata: Per primitive. Material textures, visibility buffer base index
 * DrawCounts: Per draw bucket. Number of surviving draw commands after compaction
 * CullResults: Per primitive. Number of surviving instances after frustum test
 * DrawCommands: Per primitive. Compacted each frame from DrawCommandsSource
 * VisibleInstances: Per (primitive × instance) pair. Indices into Matrices for
 *                   instances that passed the frustum test. Each primitive owns a
 *                   contiguous section sized to its instance count.
 * DrawCommandsSource: Per primitive. Immutable source of truth, set once at load
 *
 * 1:1 indexed buffers: AABB[i], Metadata[i], DrawCommands[i], DrawCommandsSource[i]
 * Flattened buffers: Map[j], VisibleInstances[j] where j spans all (primitive × instance) pairs
 *
 */

// Holds the OpenGL handles for all Shader Storage Buffer Objects
struct SSBOs
{
    GLuint map{0};
    GLuint aabb{0};
    GLuint matrices{0};
    GLuint metadata{0};
    GLuint drawCounts{0};
    GLuint cullResults{0};
    GLuint drawCommands{0};
    GLuint visibleInstances{0};
    GLuint drawCommandsSource{0};

    SSBOs() = default;
    SSBOs(const SSBOs &other) = delete;
    SSBOs &operator=(const SSBOs &other) = delete;

    SSBOs(SSBOs &&other) noexcept
        : map{std::exchange(other.map, 0)}, aabb{std::exchange(other.aabb, 0)},
          matrices{std::exchange(other.matrices, 0)}, metadata{std::exchange(other.metadata, 0)},
          drawCounts{std::exchange(other.drawCounts, 0)}, cullResults{std::exchange(other.cullResults, 0)},
          drawCommands{std::exchange(other.drawCommands, 0)},
          visibleInstances{std::exchange(other.visibleInstances, 0)},
          drawCommandsSource{std::exchange(other.drawCommandsSource, 0)}
    {
    }

    SSBOs &operator=(SSBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{map,         aabb,         matrices,         metadata,          drawCounts,
                             cullResults, drawCommands, visibleInstances, drawCommandsSource};
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            map = std::exchange(other.map, 0);
            aabb = std::exchange(other.aabb, 0);
            matrices = std::exchange(other.matrices, 0);
            metadata = std::exchange(other.metadata, 0);
            drawCounts = std::exchange(other.drawCounts, 0);
            cullResults = std::exchange(other.cullResults, 0);
            drawCommands = std::exchange(other.drawCommands, 0);
            visibleInstances = std::exchange(other.visibleInstances, 0);
            drawCommandsSource = std::exchange(other.drawCommandsSource, 0);
        }

        return *this;
    }

    ~SSBOs()
    {
        GLuint buffers[]{map,         aabb,         matrices,         metadata,          drawCounts,
                         cullResults, drawCommands, visibleInstances, drawCommandsSource};
        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */

struct GlobalVAO
{
    GLuint id{0};
    GLuint vbo{0};
    GLuint ebo{0};

    GlobalVAO() = default;
    GlobalVAO(const GlobalVAO &other) = delete;
    GlobalVAO &operator=(const GlobalVAO &other) = delete;

    GlobalVAO(GlobalVAO &&other) noexcept
        : id{std::exchange(other.id, 0)}, vbo{std::exchange(other.vbo, 0)}, ebo{std::exchange(other.ebo, 0)}
    {
    }

    GlobalVAO &operator=(GlobalVAO &&other) noexcept
    {
        if (this != &other)
        {
            glDeleteBuffers(1, &ebo);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &id);

            id = std::exchange(other.id, 0);
            vbo = std::exchange(other.vbo, 0);
            ebo = std::exchange(other.ebo, 0);
        }

        return *this;
    }

    ~GlobalVAO()
    {
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &id);
    }
};

struct UBOs
{
    GLuint camera{0};
    GLuint frustumPlanes{0};
    GLuint cascadeShadows{0};

    UBOs() = default;
    UBOs(const UBOs &other) = delete;
    UBOs &operator=(const UBOs &other) = delete;

    UBOs(UBOs &&other) noexcept
        : camera{std::exchange(other.camera, 0)}, frustumPlanes{std::exchange(other.frustumPlanes, 0)},
          cascadeShadows{std::exchange(other.cascadeShadows, 0)}
    {
    }

    UBOs &operator=(UBOs &&other) noexcept
    {
        if (this != &other)
        {
            GLuint buffers[]{camera, frustumPlanes, cascadeShadows};
            glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);

            camera = std::exchange(other.camera, 0);
            frustumPlanes = std::exchange(other.frustumPlanes, 0);
            cascadeShadows = std::exchange(other.cascadeShadows, 0);
        }

        return *this;
    }

    ~UBOs()
    {
        GLuint buffers[]{camera, frustumPlanes, cascadeShadows};
        glDeleteBuffers(static_cast<GLsizei>(std::size(buffers)), buffers);
    }
};

// Uniform Buffer Object binding points (unsigned int)
enum class BindingPointUBO : unsigned int
{
    camera,
    frustumPlanes,
    cascadeShadows
};

// Shader Storage Buffer Object binding points (unsigned int)
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
