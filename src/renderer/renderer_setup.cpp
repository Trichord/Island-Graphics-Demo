#include "renderer.h"

#include <array>
#include <cstdint>
#include <vector>

#include "aabb.h"
#include "buffers.h"
#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "primitive.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "scene_processor.h"
#include "texture_manager.h"
#include "transform.h"
#include "world_vectors.h"

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    SceneProcessor processor{scene, textureManager_};

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    std::size_t numCubemaps{processor.getNumDynamicCubemaps()};
    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    textureManager_.reserveCubemaps(numCubemaps, graphicsSettings_.reflectionResolution, colorFormat);

    textureManager_.buildAllTextures();

    // Take ownership of everything before shadow map draw call
    auto processedScene{processor.processScene()};
  
    BufferBuilder builder;
    UBOs_ = builder.buildUBOs();
    takeBuffers(processedScene);

    drawBuckets_ = std::move(processedScene.buckets);
    particleUniforms_ = std::move(processedScene.particleInfo);
    matricesBaseIndex_ = std::move(processedScene.matricesBaseIndexMap);
    cullWorkGroupSize_ = static_cast<std::uint32_t>(processedScene.numInstances);
    compactionWorkGroupSize_ = static_cast<std::uint32_t>(processedScene.numPrimitives);

    // Local copy kept for cubemap generation
    auto drawCommands = std::move(processedScene.commands);

    // Setup for cascade shadow maps
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = CascadeShadow(light, processedScene.worldAABB, nearPlane);

    // Set the light based uniforms and draw shadows
    setSceneShaderUniforms(scene);

    if (graphicsSettings_.shadowResolution > 1)
        renderShadowMaps(scene, true);

    // Assign the skybox to any ocean shaders
    borrowSkybox(processor);

    // Dynamic cubemap generation. The scene must be ready to render, except water / ocean.
    createCubemaps(scene, processor, drawCommands);

    // Update the GPU metadata buffers for the freshly created cubemaps
    processor.updateMetadataCubemaps(SSBOs_.metadata);

    // Get ready for first frame
    updateCameraUBO(scene);
}

void Renderer::takeBuffers(SceneProcessor::ProcessedScene &processed)
{
    VAO_ = std::move(processed.vao);
    SSBOs_ = std::move(processed.ssbos);

    glBindBuffer(GL_PARAMETER_BUFFER, SSBOs_.drawCounts);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, SSBOs_.drawCommands);
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

void Renderer::borrowSkybox(const SceneProcessor &processor)
{
    using enum ShaderType;

    auto skyboxHandle = processor.getSkyboxHandle();
    auto oceanInfo = processor.getShaderBucketInfo(ocean);

    if (!skyboxHandle.has_value() || !oceanInfo.has_value())
        return;

    for (Handle &oceanHandle : oceanInfo.value().handles)
        textureManager_.borrowCubemaps(oceanHandle, skyboxHandle.value());
}

void Renderer::createCubemaps(Scene &scene, const SceneProcessor &processor,
                              std::vector<DrawElementsIndirectCommand> &commands)
{
    using enum ShaderType;

    auto optWaterInfo = processor.getShaderBucketInfo(water);

    if (!optWaterInfo.has_value())
        return;

    auto &waterInfo = optWaterInfo.value();

    // Ensure reflections don't capture uninitialized other reflections or self geometry
    for (std::size_t i{waterInfo.drawCommandsStartIndex}; i < waterInfo.drawCommandsStartIndex + waterInfo.size; ++i)
        commands[i].count = 0;

    // Update the draw command buffer now that water is invisible
    GLsizeiptr drawCommandBufferSize{static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand))};
    glNamedBufferSubData(SSBOs_.drawCommands, 0, drawCommandBufferSize, commands.data());

    // Save camera settings before we make it bounce around the scene
    Camera &camera{scene.getCamera()};
    const glm::vec3 originalPos{camera.getPosition()};
    const float originalPitch{camera.getPitch()};
    const float originalYaw{camera.getYaw()};
    const float originalFOV{camera.getFOV()};
    const float originalAspect{camera.getAspectRatio()};

    // 90 degrees for cubemap
    camera.setFOV(90.0f);
    camera.setAspectRatio(1.0f);

    int faceSize{graphicsSettings_.reflectionResolution};
    glViewport(0, 0, faceSize, faceSize);

    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    RenderTarget cubeFaces{faceSize, faceSize,
                           RenderTarget::Settings{.msaa = graphicsSettings_.msaa,
                                                  .depthType = RenderTarget::DepthType::TEXTURE,
                                                  .internalFormat = colorFormat,
                                                  .depthFormat = GL_DEPTH_COMPONENT24,
                                                  .textureArray = true,
                                                  .textureArrayDepth = 6}};

    RenderTarget blitTarget{faceSize, faceSize,
                            RenderTarget::Settings{.msaa = 0,
                                                   .depthType = RenderTarget::DepthType::NONE,
                                                   .internalFormat = colorFormat,
                                                   .depthFormat = GL_NONE,
                                                   .textureArray = true,
                                                   .textureArrayDepth = 6}};

    cubeFaces.bind();

    // Water should not be instanced, limit one cubemap per model. By walking the scene instead of draw entries,
    // we ensure each model gets one cubemap
    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType != water)
            continue;

        Transform &transform{entry.transforms.front()};

        // Set the camera in the center of the water plane
        camera.setPosition(transform.getPosition());
        renderCubeFaces(scene, cubeFaces, blitTarget);

        // Assign the texture
        std::vector<int> materialIndices;
        materialIndices.reserve(entry.primitives.size());

        for (Primitive &primitive : entry.primitives)
            materialIndices.push_back(primitive.materialIndex);

        textureManager_.setCubemap(entry.handle, materialIndices, blitTarget);
    }

    // Reset
    camera.setFOV(originalFOV);
    camera.setAspectRatio(originalAspect);
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateCameraUBO(scene);

    cubeFaces.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget)
{
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    auto renderFace = [&](WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateCameraUBO(scene);
        renderScene();
    };

    Camera &camera{scene.getCamera()};

    for (int i{0}; i < 6; ++i)
    {
        target.setTextureLayer(i);
        blitTarget.setTextureLayer(i);

        target.clear();
        renderFace(faces[i], camera);

        target.blitTo(blitTarget);
    }
}