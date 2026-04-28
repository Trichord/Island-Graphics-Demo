#include <array>
#include <cstdint>
#include <vector>

#include "aabb.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "renderer.h"
#include "renderer_types.h"
#include "scene.h"
#include "shader.h"
#include "transform.h"

static void normalizePlane(glm::vec4 &vec)
{
    const float length = glm::length(glm::vec3{vec});
    vec /= length;
}

// Planes are order of: Left, Right, Top, Bottom, Near, Far
static std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix)
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

// Cull and build up the visible indices for the SSBO
void Renderer::cullScene(Scene &scene, const glm::mat4 &cullMatrix)
{
    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};

    for (DrawEntry &entry : drawEntries_)
    {
        if (entry.shaderType != ShaderType::skybox)
            cullDrawEntry(entry, planes);
        else
            cullNone(entry);
    }

    glNamedBufferSubData(visibleIndicesSSBO_, 0, static_cast<GLsizeiptr>(visibleMatrices_.size() * sizeof(GLuint)),
                         visibleMatrices_.data());
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Renderer::cullDrawEntry(DrawEntry &drawEntry, const std::array<glm::vec4, 6> &frustumPlanes)
{
    bool anyVisible{false};
    drawEntry.visibleInstances = 0;

    const AABB &modelAABB{drawEntry.primitive->aabb};
    std::vector<Transform> &transforms{*drawEntry.transforms};
    std::uint32_t visibleIndex{drawEntry.visibleIndiciesBufferBaseIndex};

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(transforms.size()); ++i)
    {
        if (modelAABB.intersects(frustumPlanes, transforms[i].getModelMatrix()))
        {
            anyVisible = true;
            visibleMatrices_[visibleIndex++] = i;
            ++drawEntry.visibleInstances;
        }
    }

    drawEntry.anyVisible = anyVisible;
}

void Renderer::cullNone(DrawEntry &drawEntry)
{
    drawEntry.anyVisible = true;
    drawEntry.visibleInstances = static_cast<int>(drawEntry.transforms->size());
    std::uint32_t visibleIndex{drawEntry.visibleIndiciesBufferBaseIndex};

    for (std::uint32_t i{0}; i < drawEntry.visibleInstances; ++i)
        visibleMatrices_[visibleIndex++] = i;
}