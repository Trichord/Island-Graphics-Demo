#include <array>
#include <vector>

#include "aabb.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "renderer.h"
#include "scene.h"
#include "shader_type.h"
#include "transform.h"

static void normalizePlane(glm::vec4 &vec)
{
    const float length = glm::length(glm::vec3{vec});
    vec /= length;
}

// Planes are order of: Left, Right, Top, Bottom, Near, Far
static std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 matrix)
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
void Renderer::cullAllModels(Scene &scene, const glm::mat4 &cullMatrix)
{
    // Reset everyting for a fresh SSBO upload
    int indicesBufferOffset{0};
    resetVisibility();

    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};

    for (DrawEntry &entry : drawEntries_)
    {
        if (entry.shaderType != ShaderType::skybox)
            cullInstanced(entry, planes, indicesBufferOffset);
        else
            cullNone(entry, indicesBufferOffset);
    }

    glNamedBufferSubData(visibleIndicesSSBO_, 0, static_cast<GLsizeiptr>(visibleMatrices_.size() * sizeof(GLuint)),
                         visibleMatrices_.data());
}

void Renderer::cullInstanced(DrawEntry &drawEntry, const std::array<glm::vec4, 6> &frustumPlanes,
                             int &ssboIndicesOffset)
{
    bool anyVisible{false};
    const AABB &modelAABB{drawEntry.primitive->aabb};
    std::vector<Transform> &transforms{*drawEntry.transforms};

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(transforms.size()); ++i)
    {
        if (modelAABB.intersects(frustumPlanes, transforms[i].getModelMatrix()))
        {
            anyVisible = true;

            ++drawEntry.visibleInstances;
            visibleMatrices_.push_back(i);
        }
    }

    if (anyVisible)
    {
        drawEntry.anyVisible = true;

        drawEntry.visibleIndicesBufferBaseIndex = ssboIndicesOffset;
        ssboIndicesOffset += drawEntry.visibleInstances;
    }
}

void Renderer::cullNone(DrawEntry &drawEntry, int &ssboIndicesOffset)
{
    drawEntry.anyVisible = true;
    drawEntry.visibleInstances = static_cast<int>(drawEntry.transforms->size());

    for (std::uint32_t i{0}; i < drawEntry.visibleInstances; ++i)
        visibleMatrices_.push_back(i);

    drawEntry.visibleIndicesBufferBaseIndex = ssboIndicesOffset;
    ssboIndicesOffset += drawEntry.visibleInstances;
}

void Renderer::resetVisibility()
{
    visibleMatrices_.clear();

    for (DrawEntry &entry : drawEntries_)
    {
        entry.anyVisible = false;
        entry.visibleInstances = 0;
        entry.visibleIndicesBufferBaseIndex = 0;
    }
}
