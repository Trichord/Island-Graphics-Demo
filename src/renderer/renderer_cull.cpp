#include <array>
#include <iostream>
#include <span>

#include "aabb.h"
#include "glm/glm.hpp"
#include "instance_data.h"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader_types.h"

// Culling range for grass
constexpr float grassCullDistance{0.4f};

void Renderer::cullRenderables(Scene &scene)
{
    const std::array<glm::vec4, 6> &frustumPlanes{scene.camera.getFrustumPlanes()};

    for (auto &[shaderType, objectList] : scene.getSceneObjects())
        for (auto &object : objectList)
        {
            using enum CullStrategy;

            CullStrategy strat{object.cullStrategy};

            switch (strat)
            {
            case MODEL:
                cullModel(object, frustumPlanes);
                break;

            case BY_MESH:
                cullByMesh(object, frustumPlanes);
                break;

            case INSTANCED:
                cullInstanced(object, scene.camera.getPosition(), frustumPlanes, false);
                break;

            case INSTANCED_DISTANCE:
                cullInstanced(object, scene.camera.getPosition(), frustumPlanes, true);
                break;

            case NONE:
                break;

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }
}

void Renderer::cullModel(RenderableData &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    const glm::mat4 &modelMatrix = renderable.transform.getModelMatrix();
    auto &object = renderable.object.get();
    const AABB &aabb = object.getAABB();

    renderable.isVisible = aabb.intersects(frustumPlanes, modelMatrix);
}

// Cull by distance is represented as a fraction of the frustum length. e.g.
// 0.5f will cull grass at half of the viewable distance
void Renderer::cullInstanced(RenderableData &renderable, const glm::vec3 &cameraPos,
                             const std::array<glm::vec4, 6> &frustumPlanes, bool cullByDistance)
{
    float nearDistance{frustumPlanes[4].w};
    float farDistance{frustumPlanes[5].w};
    bool anyVisible{false};

    const float cullDistance{nearDistance + (farDistance - nearDistance) * grassCullDistance};

    auto &object = renderable.object.get();
    std::span<InstanceData> instances = object.getInstanceData();

    for (auto &instance : instances)
    {
        const float objToCamera{glm::length(cameraPos - instance.position)};

        // Outside of distance based culling
        if (cullByDistance && objToCamera > cullDistance)
        {
            instance.isVisible = false;
            continue;
        }

        instance.isVisible = instance.worldAABB.intersectsWorldSpace(frustumPlanes);
        if (instance.isVisible)
        {
            anyVisible = true;
            double xDist{instance.position.x - cameraPos.x};
            double zDist{instance.position.z - cameraPos.z};
            instance.sortKey = xDist * xDist + zDist * zDist;
            renderable.isVisible = true;
        }
    }

    renderable.isVisible = anyVisible;

    if (anyVisible)
        updateSSBO(object);
}

void Renderer::cullByMesh(RenderableData &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    renderable.isVisible = false;

    for (auto &mesh : renderable.object.get().getMeshes())
    {
        mesh.isVisible = false;

        for (const auto &primitive : mesh.primitives)
        {
            if (primitive.getAABB().intersects(frustumPlanes, renderable.transform.getModelMatrix()))
            {
                mesh.isVisible = true;
                renderable.isVisible = true;
                break;
            }
        }
    }
}
