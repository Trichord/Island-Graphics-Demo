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
#include "renderer/textures.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/buffer_builder.h"
#include "utilities/scene_processor.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

namespace
{
std::pair<std::size_t, bool> findBucket(const std::vector<MultiDrawBucket> &buckets, ShaderType type)
{
    for (std::size_t i{0}; i < buckets.size(); ++i)
        if (buckets[i].shaderType == type)
            return std::pair<std::size_t, bool>{i, true};

    return std::pair<std::size_t, bool>{0, false};
}
} // namespace

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    BufferBuilder builder;
    UBOs_ = builder.buildUBOs();

    SceneProcessor processor{scene, textureManager_};

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    std::size_t numCubemaps{processor.getNumDynamicCubemaps()};
    GLenum colorFormat{DynamicCubemaps::internalFormat};
    textureManager_.reserveCubemaps(numCubemaps, graphicsSettings_.reflectionResolution, colorFormat);

    textureManager_.buildAllTextures();
    SceneProcessor::ProcessedScene processed{processor.processScene()};

    // Take ownership of everything before shadow map draw call
    takeGPUObjects(processed);
    takeBufferData(processed);

    // Extract temporaries
    AABB sceneAABB{processed.sceneDescriptor.worldAABB};
    std::vector<std::set<Handle>> bucketHandles{std::move(processed.bufferDescriptor.drawBucketHandles)};

    setupComputeShaders(processed);

    // Setup for cascade shadow maps
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = CascadeShadow(light, sceneAABB, nearPlane);

    // Set the light based uniforms and draw shadows
    setSceneShaderUniforms(scene);

    if (graphicsSettings_.shadowResolution > 1)
        renderShadowMaps(scene, true);

    // Assign the skybox to any ocean shaders
    borrowSkybox(drawBuckets_, bucketHandles);

    // Dynamic cubemap generation. The scene must be ready to render, except water / ocean.
    createCubemaps(scene, drawBuckets_);

    // Update the GPU metadata buffers for the freshly created cubemaps
    processor.updateMetadataCubemaps(SSBOs_.metadata);

    // Get ready for first frame
    updateCameraUBO(scene);
}

void Renderer::takeGPUObjects(SceneProcessor::ProcessedScene &processed)
{
    VAOs_ = std::move(processed.VAOs);
    SSBOs_ = std::move(processed.SSBOs);

    glBindBuffer(GL_PARAMETER_BUFFER, SSBOs_.drawCounts);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, SSBOs_.drawCommands);
}

void Renderer::takeBufferData(SceneProcessor::ProcessedScene &processed)
{
    drawBuckets_ = std::move(processed.drawBuckets);
    matricesBaseIndex_ = std::move(processed.bufferDescriptor.matricesBaseIndexMap);
}

void Renderer::setupComputeShaders(const SceneProcessor::ProcessedScene &processed)
{
    cullWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptor.numInstances);
    compactionWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptor.numPrimitives);
    particleWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptor.numParticles);
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

void Renderer::borrowSkybox(const std::vector<MultiDrawBucket> &drawBuckets,
                            const std::vector<std::set<Handle>> &bucketHandles)
{
    using enum ShaderType;

    auto [oceanIndex, oceanFound] = findBucket(drawBuckets, ocean);
    auto [skyboxIndex, skyboxFound] = findBucket(drawBuckets, skybox);

    if (!oceanFound || !skyboxFound)
        return;

    std::set<Handle> oceanHandles{bucketHandles[oceanIndex]};
    std::set<Handle> skyboxHandles{bucketHandles[skyboxIndex]};

    // Should't have more than one skybox. Undefined if there is.
    Handle skyboxHandle{*skyboxHandles.begin()};

    for (const Handle &oceanHandle : oceanHandles)
        textureManager_.borrowCubemaps(oceanHandle, skyboxHandle);
}

void Renderer::createCubemaps(Scene &scene, const std::vector<MultiDrawBucket> &drawBuckets)
{
    using enum ShaderType;

    auto [waterIndex, found] = findBucket(drawBuckets, water);

    if (!found)
        return;

    // Find which parameter buffer index to 0, so that water never tries to self-draw
    std::size_t parameterBufferIndex{0};
    for (std::size_t i{0}; i < drawBuckets.size(); ++i)
        if (drawBuckets[i].shaderType == water)
        {
            parameterBufferIndex = i;
            break;
        }

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

    GLenum colorFormat{DynamicCubemaps::internalFormat};
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
        renderCubeFaces(scene, cubeFaces, blitTarget, parameterBufferIndex);

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

void Renderer::renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget,
                               std::size_t parameterBufferIndex)
{
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    auto renderFace = [&](WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateCameraUBO(scene);
        cullScene(camera.getViewProjectionMatrix());

        GLuint zero{0};
        glNamedBufferSubData(SSBOs_.drawCounts, static_cast<GLsizeiptr>(parameterBufferIndex * sizeof(std::uint32_t)),
                             sizeof(GLuint), &zero);

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