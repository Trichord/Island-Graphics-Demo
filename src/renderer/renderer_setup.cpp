#include "renderer/renderer.h"

#include <array>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/cascade_shadow.h"
#include "renderer/render_target.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/buffer_builder.h"
#include "utilities/scene_processor.h"
#include "utilities/shader_bucket_hash.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    BufferBuilder builder;
    UBOs_ = builder.buildUBOs();

    SceneProcessor processor{scene, textureManager_};

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    std::size_t numCubemaps{processor.getNumDynamicCubemaps()};
    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    textureManager_.reserveCubemaps(numCubemaps, graphicsSettings_.reflectionResolution, colorFormat);

    textureManager_.buildAllTextures();
    SceneProcessor::ProcessedScene processed{processor.processScene()};

    // Take ownership of everything before shadow map draw call
    takeGPUObjects(processed);
    takeBufferData(processed);

    // Extract temporaries
    std::vector<DrawElementsIndirectCommand> commands{std::move(processed.commands)};
    AABB sceneAABB{processed.sceneData.worldAABB};
    auto bucketMap{std::move(processed.bucketMap)};
    std::vector<std::size_t> vertexCounts{std::move(processed.vertexCounts)};

    setLocalGroupSizes(processed, commands, vertexCounts, bucketMap);

    // Setup for cascade shadow maps
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = CascadeShadow(light, sceneAABB, nearPlane);

    // Set the light based uniforms and draw shadows
    setSceneShaderUniforms(scene);

    if (graphicsSettings_.shadowResolution > 1)
        renderShadowMaps(scene, true);

    // Assign the skybox to any ocean shaders
    borrowSkybox(bucketMap);

    // Dynamic cubemap generation. The scene must be ready to render, except water / ocean.
    createCubemaps(scene, commands, bucketMap);

    // Update the GPU metadata buffers for the freshly created cubemaps
    processor.updateMetadataCubemaps(SSBOs_.metadata);

    // Get ready for first frame
    updateCameraUBO(scene);
}

void Renderer::takeGPUObjects(SceneProcessor::ProcessedScene &processed)
{
    VAOs_ = std::move(processed.vao);
    SSBOs_ = std::move(processed.ssbos);

    glBindBuffer(GL_PARAMETER_BUFFER, SSBOs_.drawCounts);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, SSBOs_.drawCommands);
}

void Renderer::takeBufferData(SceneProcessor::ProcessedScene &processed)
{
    drawBuckets_ = std::move(processed.bufferData.buckets);
    matricesBaseIndex_ = std::move(processed.bufferData.matricesBaseIndexMap);
}

void Renderer::setLocalGroupSizes(const SceneProcessor::ProcessedScene &processed,
                                  const std::vector<DrawElementsIndirectCommand> &commands,
                                  const std::vector<std::size_t> vertexCounts, const BucketMap &bucketMap)
{
    using enum ShaderType;

    cullWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneData.numInstances);
    compactionWorkGroupSize_ = static_cast<std::uint32_t>(commands.size());

    for (const auto &[key, bucket] : bucketMap)
    {
        if (key.first == particles)
        {
            for (std::size_t i{bucket.startIndex}; i <= bucket.endIndex; ++i)
                particleWorkGroupSize_ += static_cast<std::uint32_t>(vertexCounts[i]);
        }
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    const DirectionalLight &light{scene.getGlobalLight()};
    const float fov{scene.getCamera().getFOV()};

    for (auto &shader : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", light.getColor());
        shader.setVec3("lightDir", light.getDirection());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());

        shader.setFloat("fov", fov);
    }
}

void Renderer::borrowSkybox(const BucketMap &bucketMap)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    auto skyboxIter = bucketMap.find(ShaderBucket{skybox, Opaque});
    auto oceanIter = bucketMap.find(ShaderBucket{ocean, Opaque});

    if (skyboxIter == bucketMap.end() || oceanIter == bucketMap.end())
        return;

    if (skyboxIter->second.handles.empty())
        return;

    Handle skyboxHandle{*skyboxIter->second.handles.begin()};

    for (const Handle &oceanHandle : oceanIter->second.handles)
        textureManager_.borrowCubemaps(oceanHandle, skyboxHandle);
}

void Renderer::createCubemaps(Scene &scene, const std::vector<DrawElementsIndirectCommand> &commandsSource,
                              const BucketMap &bucketMap)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    auto waterIter = bucketMap.find(ShaderBucket{water, Blend});

    if (waterIter == bucketMap.end())
        return;

    std::size_t commandsStart{waterIter->second.startIndex};
    std::size_t commandsEnd{waterIter->second.endIndex};

    std::vector<DrawElementsIndirectCommand> commands = commandsSource;

    // Ensure reflections don't capture uninitialized other reflections or self geometry
    for (std::size_t i{commandsStart}; i <= commandsEnd; ++i)
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
                                                  .depthType = RenderTarget::DepthType::texture,
                                                  .internalFormat = colorFormat,
                                                  .depthFormat = GL_DEPTH_COMPONENT24,
                                                  .textureArray = true,
                                                  .textureArrayDepth = 6}};

    RenderTarget blitTarget{faceSize, faceSize,
                            RenderTarget::Settings{
                                .internalFormat = colorFormat,
                                .textureArray = true,
                                .textureArrayDepth = 6,
                            }};

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