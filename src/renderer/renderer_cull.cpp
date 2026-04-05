#include <array>
#include <iostream>
#include <span>

#include "aabb.h"
#include "camera.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader_type.h"

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
void Renderer::cullRenderables(Scene &scene, const glm::mat4 &cullMatrix)
{
    // Reset everyting for a fresh SSBO upload
    GLuint indicesBufferOffset{0};
    instanceIndices_.clear();
    resetVisibility();

    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};

    for (auto &[_, drawEntries] : drawEntriesByShader_)
    {
        for (DrawEntry &drawEntry : drawEntries)
        {
            using enum CullMethod;

            switch (drawEntry.cullMethod)
            {
            case INSTANCED:
                cullInstanced(drawEntry, planes, indicesBufferOffset);
                break;

            case TERRAIN:
                cullByMesh(drawEntry, planes, indicesBufferOffset);
                break;

            case NONE: {
                cullNone(drawEntry, indicesBufferOffset);
                break;
            }

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }
    }

    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(GLuint), instanceIndices_.data());
}

void Renderer::cullInstanced(DrawEntry &drawEntry, const std::array<glm::vec4, 6> &frustumPlanes,
                             GLuint &ssboIndicesOffset)
{
    bool anyVisible{false};
    Renderable &renderable{drawEntry.renderableRef.get()};
    const std::vector<BoundedTransform> &instances{drawEntry.instancesRef.get()};

    for (uint32_t i{0}; i < instances.size(); ++i)
    {
        if (instances[i].worldAABB.intersectsWorldSpace(frustumPlanes))
        {
            anyVisible = true;

            ++drawEntry.visibleInstances;
            instanceIndices_.push_back(i);
        }
    }

    if (anyVisible)
    {
        drawEntry.isVisible = true;
        renderable.setMeshVisibility(true);

        drawEntry.indicesBufferIndex = ssboIndicesOffset;
        ssboIndicesOffset += drawEntry.visibleInstances;
    }
}

void Renderer::cullByMesh(DrawEntry &drawEntry, const std::array<glm::vec4, 6> &frustumPlanes,
                          GLuint &ssboIndicesOffset)
{
    bool anyVisible{false};
    Renderable &renderable{drawEntry.renderableRef.get()};
    std::vector<BoundedTransform> &instances{drawEntry.instancesRef.get()};

    for (uint32_t i{0}; i < instances.size(); ++i)
    {
        bool instanceVisible{false};

        for (auto &mesh : renderable.getMeshes())
        {
            for (auto &primitive : mesh.primitives)
            {
                if (primitive.getAABB().intersects(frustumPlanes, instances[i].transform.getModelMatrix()))
                {
                    anyVisible = true;
                    mesh.isVisible = true;
                    instanceVisible = true;
                }
            }
        }

        if (instanceVisible)
        {
            instanceIndices_.push_back(i);
            ++drawEntry.visibleInstances;
        }
    }

    if (anyVisible)
    {
        drawEntry.isVisible = true;
        drawEntry.indicesBufferIndex = ssboIndicesOffset;
        ssboIndicesOffset += drawEntry.visibleInstances;
    }
}

void Renderer::resetVisibility()
{
    for (auto &[_, drawEntries] : drawEntriesByShader_)
    {
        for (DrawEntry &drawEntry : drawEntries)
        {
            Renderable &renderable{drawEntry.renderableRef.get()};
            GLuint instanceCount{static_cast<GLuint>(drawEntry.instancesRef.get().size())};

            drawEntry.isVisible = false;
            drawEntry.visibleInstances = 0;
            drawEntry.indicesBufferIndex = 0;

            renderable.setMeshVisibility(false);
        }
    }
}

void Renderer::cullNone(DrawEntry &drawEntry, GLuint &ssboIndicesOffset)
{
    Renderable &renderable{drawEntry.renderableRef.get()};
    std::vector<BoundedTransform> &instances{drawEntry.instancesRef.get()};

    renderable.setMeshVisibility(true);
    drawEntry.isVisible = true;
    drawEntry.visibleInstances = static_cast<GLuint>(instances.size());

    for (GLuint i{0}; i < static_cast<GLuint>(instances.size()); ++i)
        instanceIndices_.push_back(i);

    drawEntry.indicesBufferIndex = ssboIndicesOffset;
    ssboIndicesOffset += drawEntry.visibleInstances;
}
