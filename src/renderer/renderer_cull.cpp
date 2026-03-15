#include <array>
#include <iostream>
#include <span>

#include "aabb.h"
#include "camera.h"
#include "glm/glm.hpp"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader_type.h"

void Renderer::cullRenderables(Scene &scene)
{
    GLuint indicesOffset{0};
    instanceIndices_.clear();

    Camera &camera{scene.getCamera()};
    const std::array<glm::vec4, 6> &frustumPlanes{camera.getFrustumPlanes()};

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
        {
            using enum CullMethod;

            CullMethod cull{renderable.properties.cullMethod};

            switch (cull)
            {
            case INSTANCED:
                cullInstanced(renderable, camera.getPosition(), frustumPlanes, indicesOffset);
                break;

            case TERRAIN:
                cullByMesh(renderable, frustumPlanes, indicesOffset);
                break;

            case NONE: {
                cullNone(renderable, indicesOffset);
                break;
            }

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }

    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(std::uint32_t), instanceIndices_.data());
}

void Renderer::cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                             const std::array<glm::vec4, 6> &frustumPlanes, GLuint &currentOffset)
{
    bool anyVisible{false};
    auto &object = renderable.objectRef.get();
    auto &instances = renderable.properties.instances;
    auto &offsets{modelBaseIndices_[object.getHandle()]};

    // Build up the visible indices for the SSBO
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

        offsets.indicesOffset = currentOffset;

        currentOffset += renderable.properties.visibleInstances;
    }
}

void Renderer::cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes, GLuint &currentOffset)
{
    bool anyVisible{false};
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;
    auto &instances = properties.instances;
    auto &offsets{modelBaseIndices_[object.getHandle()]};

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
                    properties.isVisible = true;
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
        renderable.properties.isVisible = true;
        object.setMeshVisibility(true);

        offsets.indicesOffset = currentOffset;

        currentOffset += renderable.properties.visibleInstances;
    }
}

void Renderer::resetVisibility(Scene &scene)
{
    for (auto &[shader, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            using enum ShaderType;
            using enum CullMethod;

            ObjectProperties &properties{renderable.properties};
            auto &object = renderable.objectRef.get();
            auto &offsets{modelBaseIndices_[object.getHandle()]};

            offsets.indicesOffset = 0;
            properties.isVisible = false;
            properties.visibleInstances = 0;
            object.setMeshVisibility(false);
        }
}

void Renderer::cullNone(Renderable &renderable, GLuint &currentOffset)
{
    auto &properties = renderable.properties;
    auto &object = renderable.objectRef.get();
    auto &instances = renderable.properties.instances;
    auto &offsets{modelBaseIndices_[object.getHandle()]};

    object.setMeshVisibility(true);
    properties.isVisible = true;
    properties.visibleInstances = static_cast<GLuint>(instances.size());

    for (GLuint i{0}; i < static_cast<GLuint>(instances.size()); ++i)
        instanceIndices_.push_back(i);

    offsets.indicesOffset = currentOffset;
    currentOffset += properties.visibleInstances;
}
