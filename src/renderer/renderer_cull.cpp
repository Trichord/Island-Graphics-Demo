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

            case BY_MESH:
                cullByMesh(renderable, frustumPlanes);
                break;

            case INSTANCED:
                cullInstanced(renderable, camera.getPosition(), frustumPlanes, false);
                break;

            case INSTANCED_DISTANCE:
                cullInstanced(renderable, camera.getPosition(), frustumPlanes, true);
                break;

            case NONE:
                break;

            default:
                std::cerr << "ERROR: unknown enum in cull strategy" << std::endl;
            }
        }
}

void Renderer::cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.object.get();
    auto &properties = renderable.properties;

    const glm::mat4 &modelMatrix = properties.transform.getModelMatrix();
    const AABB &aabb = object.getAABB();

    properties.isVisible = aabb.intersects(frustumPlanes, modelMatrix);
}

// Cull by distance is represented as a fraction of the frustum length. e.g.
// 0.5f will cull grass at half of the viewable distance. For water reflections, we can ignore the top plane if the
// camera is within [-45.0, 45.0] degrees, otherwise reflected objects get culled too early.
void Renderer::cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
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
            float xDist{instance.position.x - cameraPos.x};
            float zDist{instance.position.z - cameraPos.z};
            instance.sortKey = xDist * xDist + zDist * zDist;
            renderable.properties.isVisible = true;
        }
    }

    renderable.properties.isVisible = anyVisible;

    if (anyVisible)
        updateSSBO(object);
}

void Renderer::cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.object.get();
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
