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
    float length = glm::length(glm::vec3{vec});
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
    resetVisibility(scene);

    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
        {
            using enum CullMethod;

            CullMethod cull{renderable.properties.cullMethod};

            switch (cull)
            {
            case INSTANCED:
                cullInstanced(renderable, planes, indicesBufferOffset);
                break;

            case TERRAIN:
                cullByMesh(renderable, planes, indicesBufferOffset);
                break;

            case NONE: {
                cullNone(renderable, indicesBufferOffset);
                break;
            }

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }

    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(GLuint), instanceIndices_.data());
}

void Renderer::cullInstanced(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes,
                             GLuint &ssboIndicesOffset)
{
    bool anyVisible{false};
    auto &object = renderable.objectRef.get();
    auto &instances = renderable.properties.instances;
    auto &modelBufferOffsets{modelBaseIndices_[object.getHandle()]};

    for (uint32_t i{0}; i < instances.size(); ++i)
    {
        if (instances[i].worldAABB.intersectsWorldSpace(frustumPlanes))
        {
            anyVisible = true;

            ++renderable.properties.visibleInstances;
            instanceIndices_.push_back(i);
        }
    }

    if (anyVisible)
    {
        renderable.properties.isVisible = true;
        object.setMeshVisibility(true);

        modelBufferOffsets.indicesOffset = ssboIndicesOffset;

        ssboIndicesOffset += renderable.properties.visibleInstances;
    }
}

void Renderer::cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes,
                          GLuint &ssboIndicesOffset)
{
    bool anyVisible{false};
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;
    auto &instances = properties.instances;
    auto &modelBufferOffsets{modelBaseIndices_[object.getHandle()]};

    for (uint32_t i{0}; i < instances.size(); ++i)
    {
        bool instanceVisible{false};

        for (auto &mesh : object.getMeshes())
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
            ++renderable.properties.visibleInstances;
        }
    }

    if (anyVisible)
    {
        properties.isVisible = true;
        modelBufferOffsets.indicesOffset = ssboIndicesOffset;
        ssboIndicesOffset += renderable.properties.visibleInstances;
    }
}

void Renderer::resetVisibility(Scene &scene)
{
    for (auto &[shader, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            auto &object = renderable.objectRef.get();
            auto &offsets{modelBaseIndices_[object.getHandle()]};
            ObjectProperties &properties{renderable.properties};
            GLuint instanceCount{static_cast<GLuint>(properties.instances.size())};

            properties.isVisible = false;
            properties.visibleInstances = 0;
            object.setMeshVisibility(false);
            offsets.indicesOffset = 0;
        }
}

void Renderer::cullNone(Renderable &renderable, GLuint &ssboIndicesOffset)
{
    auto &properties = renderable.properties;
    auto &object = renderable.objectRef.get();
    auto &instances = renderable.properties.instances;
    auto &modelBufferOffsets{modelBaseIndices_[object.getHandle()]};

    object.setMeshVisibility(true);
    properties.isVisible = true;
    properties.visibleInstances = static_cast<GLuint>(instances.size());

    for (GLuint i{0}; i < static_cast<GLuint>(instances.size()); ++i)
        instanceIndices_.push_back(i);

    modelBufferOffsets.indicesOffset = ssboIndicesOffset;
    ssboIndicesOffset += properties.visibleInstances;
}
