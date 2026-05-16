#include "renderer.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "glad/glad.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
#include "texture_manager.h"

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    SceneTotals totals{getSceneTotals(scene)};

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    int resolution{getReflectionResolution()};
    int numCubemaps{static_cast<int>(totals.dynamicCubemaps)};
    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    textureManager_.reserveCubemaps(numCubemaps, resolution, colorFormat);

    textureManager_.buildAllTextures();

    // Create DrawEntries for building the draw command buffer
    std::vector<DrawEntry> drawEntries{createDrawEntries(scene, totals)};
    std::ranges::sort(drawEntries, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    // Build draw commands and sort them into buckets for each MDI call
    drawBuckets_ = createDrawBuckets(drawEntries);
    std::vector<DrawElementsIndirectCommand> commands{createDrawCommandBuffer(drawEntries, totals)};

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
    setMetadataMaterials(textureManager_, drawEntries, metadata);

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
    createCubemaps(scene, commands, drawEntries);

    // Push the new cubemaps into metadata
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