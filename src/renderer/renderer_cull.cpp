#include <array>
#include <iostream>
#include <span>

#include "aabb.h"
#include "camera.h"
#include "glm/glm.hpp"
#include "instance_data.h"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader_types.h"

void Renderer::cullRenderables(Scene &scene)
{
    indicesOffset = 0;
    instanceIndices.clear();

    Camera &camera{scene.getCamera()};
    const std::array<glm::vec4, 6> &frustumPlanes{camera.getFrustumPlanes()};

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
        {
            using enum CullType;

            CullType cull{renderable.properties.cullType};

            switch (cull)
            {
            case MODEL:
                cullModel(renderable, frustumPlanes);
                break;

            case TERRAIN:
                cullByMesh(renderable, frustumPlanes);
                break;

            case INSTANCED:
                cullInstanced(renderable, camera.getPosition(), frustumPlanes);
                break;

            case NONE: {
                auto &object = renderable.objectRef.get();
                object.setVisibility(true);
                renderable.properties.isVisible = true;
                break;
            }

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }

    glNamedBufferSubData(indicesSSBO, 0, instanceIndices.size() * sizeof(std::uint32_t), instanceIndices.data());
}

void Renderer::cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;

    const glm::mat4 &modelMatrix = properties.transform.getModelMatrix();
    const AABB &aabb = object.getAABB();

    properties.isVisible = aabb.intersects(frustumPlanes, modelMatrix);

    if (properties.isVisible)
        object.setVisibility(true);
}

void Renderer::cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                             const std::array<glm::vec4, 6> &frustumPlanes)
{
    bool anyVisible{false};

    auto &object = renderable.objectRef.get();
    std::span<InstancedData> instances = object.getInstanceData();

    for (uint32_t i{0}; i < instances.size(); ++i)
    {
        if (instances[i].worldAABB.intersectsWorldSpace(frustumPlanes))
        {
            anyVisible = true;

            ++renderable.properties.visibleInstances;
            instanceIndices.push_back(i);
        }
    }

    if (anyVisible)
    {

        renderable.properties.isVisible = true;
        object.setVisibility(true);

        // Update render's count for the draw call
        renderable.properties.instancedIndicesOffset = indicesOffset;

        // Keep track of where each model's instances start in the vector of indices
        indicesOffset += renderable.properties.visibleInstances;
    }
}

void Renderer::cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;

    properties.isVisible = false;

    for (auto &mesh : object.getMeshes())
    {
        mesh.isVisible = false;

        for (const auto &primitive : mesh.primitives)
        {
            if (primitive.getAABB().intersects(frustumPlanes, properties.transform.getModelMatrix()))
            {
                mesh.isVisible = true;
                properties.isVisible = true;
                break;
            }
        }
    }
}

void Renderer::resetVisibility(Scene &scene)
{
    for (auto &[shader, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            using enum ShaderType;
            using enum CullType;

            ObjectProperties &properties{renderable.properties};
            auto &object = renderable.objectRef.get();

            if (properties.isVisible)
            {

                properties.isVisible = false;
                properties.visibleInstances = 0;
                properties.instancedIndicesOffset = 0;
                object.setVisibility(false);
            }
        }
}
