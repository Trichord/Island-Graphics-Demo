#include "renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    SceneTotals totals{getSceneTotals(scene)};

    // 2D Textures
    textureManager_.texturesReserved();
    textureManager_.buildTextures();

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    int resolution{getReflectionResolution()};
    int numCubemaps{static_cast<int>(totals.dynamicCubemaps)};
    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    SamplerArrayEntry reservedEntry{textureManager_.reserveCubemaps(numCubemaps, resolution, colorFormat)};

    textureManager_.cubemapsReserved();
    textureManager_.buildCubemaps();

    // Create DrawEntries for building the draw command buffer
    std::vector<DrawEntry> drawEntries{createDrawEntries(scene, totals)};
    std::ranges::sort(drawEntries, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    // Build draw commands and sort them into MDI Count buckets
    std::vector<DrawElementsIndirectCommand> commands{buildDrawCommandBuffer(drawEntries, totals)};
    drawBuckets_ = createDrawBuckets(drawEntries);

    // Culling setup
    std::vector<AABB> aabbs{createAABBs(drawEntries, totals)};
    cullWorkGroupSize_ = static_cast<std::uint32_t>(totals.visibleInstances);
    compactionWorkGroupSize_ = static_cast<std::uint32_t>(totals.primitives);

    // GPU Resources
    createBuffers(commands, aabbs, totals);
    createVAO(drawEntries, totals);

    // Metadata buffer setup
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(drawEntries.size());
    populateMatricesSSBO(drawEntries, metadata, totals);
    setVisibleInstancesBaseIndex(drawEntries, metadata);
    setMetadataMaterials(drawEntries, metadata);

    glNamedBufferSubData(metadataBuffer_, 0, metadata.size() * sizeof(PrimitiveMetadata), metadata.data());

    // Must be done after GPU buffers are complete
    createBufferMap(drawEntries, metadata, totals);

    // Setup for cascade shadow maps
    AABB worldAABB{getSceneBounds(scene)};
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = CascadeShadow(light, worldAABB, nearPlane);

    // Set the light based uniforms and draw shadows
    setSceneShaderUniforms(scene);

    if (graphicsSettings_.shadowResolution > 1)
        renderShadowMaps(scene, true);

    // Assign the skybox to any ocean shaders
    borrowSkybox(drawEntries);

    // Dynamic cubemap generation. The scene must be ready to render, except water / ocean.
    createCubemaps(scene, commands, drawEntries, reservedEntry);

    // Push the new cubemaps through
    updateMetadataCubemaps(drawEntries, metadata);

    // Get ready for first frame
    updateCameraUBO(scene);
    threadPool_.stopAllThreads();
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    const DirectionalLight &light{scene.getGlobalLight()};

    for (auto &shader : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", light.getColor());
        shader.setVec3("lightDir", light.getDirection());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());
    }
}

void Renderer::updateMetadataCubemaps(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        using enum ShaderType;

        if (entries[i].shaderType == water || entries[i].shaderType == ocean)
        {
            const DrawEntry &drawEntry{entries[i]};
            PrimitiveMetadata &metaEntry{metadata[i]};

            const int matIndex{drawEntry.primitive->materialIndex};
            const std::vector<Material> &materials{textureManager_.getMaterials(drawEntry.handle)};
            const Material &material{materials[matIndex]};

            metaEntry.cubemapArray = material.cubemap.array;
            metaEntry.cubemapLayer = material.cubemap.layer;

            glNamedBufferSubData(metadataBuffer_, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                                 static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
        }
    }
}

void Renderer::populateMatricesSSBO(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata,
                                    const SceneTotals &totals)
{
    std::uint32_t matricesIndex{0};
    std::vector<glm::mat4> modelMatrices;
    modelMatrices.reserve(totals.matrices);

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        auto [iter, inserted] = transformsByHandle_.try_emplace(entries[i].handle);

        if (inserted)
        {
            std::vector<Transform> &transforms{*entries[i].transforms};

            for (Transform &transform : transforms)
                modelMatrices.emplace_back(transform.getModelMatrix());

            iter->second = {matricesIndex, &transforms};

            matricesIndex += static_cast<std::uint32_t>(transforms.size());
        }
    }

    glNamedBufferSubData(matricesBuffer_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
}

void Renderer::setMetadataMaterials(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        const DrawEntry &entry{entries[i]};
        PrimitiveMetadata &metaEntry{metadata[i]};

        const int matIndex{entry.primitive->materialIndex};
        const std::vector<Material> &materials{textureManager_.getMaterials(entry.handle)};
        const Material &material{materials[matIndex]};

        if (matIndex < 0 || matIndex >= static_cast<int>(materials.size()))
        {
            std::cerr << "Material index out of bound for handle " << entry.handle << std::endl;
            throw std::runtime_error("Material index out of bounds");
        }

        metaEntry.albedoArray = material.albedo.array;
        metaEntry.albedoLayer = material.albedo.layer;

        metaEntry.armArray = material.ARM.array;
        metaEntry.armLayer = material.ARM.layer;

        metaEntry.normalArray = material.normal.array;
        metaEntry.normalLayer = material.normal.layer;

        metaEntry.cubemapArray = material.cubemap.array;
        metaEntry.cubemapLayer = material.cubemap.layer;
    }
}
