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
#include "shader_type.h"

void Renderer::cullRenderables(Scene &scene)
{
    indicesOffset_ = 0;
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

    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(std::uint32_t), instanceIndices_.data());
}

void Renderer::cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;

    if (properties.isInstanced())
        return;

    Transform &transform{std::get<Transform>(properties.transform)};

    const glm::mat4 &modelMatrix{transform.getModelMatrix()};
    const AABB &aabb{object.getAABB()};

    properties.isVisible = aabb.intersects(frustumPlanes, modelMatrix);

    if (properties.isVisible)
        object.setVisibility(true);
}

void Renderer::cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                             const std::array<glm::vec4, 6> &frustumPlanes)
{
    bool anyVisible{false};

    auto &object = renderable.objectRef.get();
    auto &instances = std::get<std::vector<InstancedData>>(renderable.properties.transform);

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
        object.setVisibility(true);

        // Update render's count for the draw call
        renderable.properties.instancedIndicesOffset = indicesOffset_;

        // Keep track of where each model's instances start in the vector of indices
        indicesOffset_ += renderable.properties.visibleInstances;
    }
}

void Renderer::cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes)
{
    auto &object = renderable.objectRef.get();
    auto &properties = renderable.properties;

    properties.isVisible = false;
    Transform &transform{std::get<Transform>(properties.transform)};

    for (auto &mesh : object.getMeshes())
    {
        mesh.isVisible = false;

        for (const auto &primitive : mesh.primitives)
        {
            if (primitive.getAABB().intersects(frustumPlanes, transform.getModelMatrix()))
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
            using enum CullMethod;

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
