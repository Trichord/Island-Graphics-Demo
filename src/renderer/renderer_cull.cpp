#include <array>
#include <cstdint>
#include <vector>

#include "aabb.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "renderer.h"
#include "renderer_types.h"

void normalizePlane(glm::vec4 &vec)
{
    const float length = glm::length(glm::vec3{vec});
    vec /= length;
}

// Planes are order of: Left, Right, Top, Bottom, Near, Far
std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix)
{
    std::array<glm::vec4, 6> planes;
    const glm::vec4 row0{glm::row(matrix, 0)};
    const glm::vec4 row1{glm::row(matrix, 1)};
    const glm::vec4 row2{glm::row(matrix, 2)};
    const glm::vec4 row3{glm::row(matrix, 3)};

    // Left
    planes[0] = row3 + row0;

    // Right
    planes[1] = row3 - row0;

    // Top
    planes[2] = row3 - row1;

    // Bottom
    planes[3] = row3 + row1;

    // Near
    planes[4] = row3 + row2;

    // Far
    planes[5] = row3 - row2;

    for (auto &vec : planes)
        normalizePlane(vec);

    return planes;
}

void Renderer::compactBuffer()
{
    // Clear the per MDI draw call count
    for (std::uint32_t &count : drawCounts_)
        count = 0;

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(cullResults_.size()); ++i)
    {
        if (cullResults_[i] == 0)
            continue;

        std::uint32_t visibilityBufferBaseIndex{
            metadata_[drawCommands_[i].baseInstance].visibleInstancesBufferBaseIndex};
        BufferMap map{bufferMap_[visibilityBufferBaseIndex]};

        // Use the per-mdi draw call as a write index. As we write dracommands, the final number for the draw call
        // increases, so we can take advantage of that as our new offset
        std::uint32_t writeIndex{map.drawBucketBaseIndex + drawCounts_[map.drawBucket]};

        drawCommands_[writeIndex] = drawCommandsSource_[i];
        drawCommands_[writeIndex].instanceCount = cullResults_[i];
        ++drawCounts_[map.drawBucket];
    }
}

void Renderer::cullScene(const glm::mat4 &cullMatrix)
{
    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};

    // "Compute shader 1 - culling"
    for (const DrawElementsIndirectCommand &command : drawCommandsSource_)
    {
        std::uint32_t visibleInstancesBufferBaseIndex{metadata_[command.baseInstance].visibleInstancesBufferBaseIndex};
        BufferMap map{bufferMap_[visibleInstancesBufferBaseIndex]};

        // Clear previous cull result
        std::uint32_t &counter{cullResults_[map.sharedBaseIndex]};
        counter = 0;

        for (std::uint32_t i{0}; i < command.instanceCount; ++i)
            cullVisibilityIndex(visibleInstancesBufferBaseIndex, visibleInstancesBufferBaseIndex + i, counter, planes);
    }

    // "Compute shader 2 - command compaction"
    compactBuffer();

    glNamedBufferSubData(visibleInstancesBuffer_, 0, static_cast<GLsizeiptr>(visibleInstances_.size() * sizeof(GLuint)),
                         visibleInstances_.data());
    glNamedBufferSubData(drawCommandsBuffer_, 0,
                         static_cast<GLsizeiptr>(drawCommands_.size() * sizeof(DrawElementsIndirectCommand)),
                         drawCommands_.data());

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Renderer::cullVisibilityIndex(std::uint32_t baseVisibilityIndex, std::uint32_t visibilityIndex,
                                   std::uint32_t &counter, const std::array<glm::vec4, 6> &frustumPlanes)
{
    BufferMap map{bufferMap_[visibilityIndex]};
    const AABB &aabb{aabbs_[map.sharedBaseIndex]};
    const glm::mat4 matrix{matrices_[map.matricesIndex]};

    if (aabb.intersects(frustumPlanes, matrix))
    {
        visibleInstances_[baseVisibilityIndex + counter] = map.matricesIndex;
        ++counter;
    }
}