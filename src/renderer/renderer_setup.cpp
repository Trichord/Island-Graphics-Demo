#include "renderer/renderer.h"

#include <array>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <set>
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

std::size_t getNumDynamicCubemaps(const Scene &scene)
{
    std::size_t cubemaps{0};

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
        if (entry.shaderType == ShaderType::water)
            ++cubemaps;

    return cubemaps;
}
} // namespace

void Renderer::initializeResources(Scene &scene)
{
    if (scene.getAllSceneEntries().empty())
        return;

    BufferBuilder builder;
    UBOs_ = builder.buildUBOs();

    const bool shadowsEnabled{graphicsSettings_.shadowResolution > 1};

    // Cubemaps. Reserve space up front for any load-time generated cubemaps
    std::size_t numCubemaps{getNumDynamicCubemaps(scene)};
    GLenum colorFormat{DynamicCubemaps::internalFormat};
    textureManager_.reserveCubemaps(numCubemaps, graphicsSettings_.reflectionResolution, colorFormat);

    textureManager_.buildAllTextures();

    SceneProcessor::ProcessedScene processed{SceneProcessor::processScene(scene, textureManager_, shadowsEnabled)};

    // Take ownership of everything before shadow map draw call
    takeGPUObjects(processed);
    takeBufferData(processed);

    // Extract temporaries
    AABB sceneAABB{processed.sceneDescriptors.worldAABB};
    std::vector<std::set<Handle>> bucketHandles{std::move(processed.bufferDescriptors.drawBucketHandles)};
    std::vector<std::uint32_t> metadataBridge{std::move(processed.bufferDescriptors.metadataBridge)};

    // Setup for cascade shadow maps
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = {light, sceneAABB, nearPlane};

    // Deferred shader loading so we can test the scene for animated models
    std::cerr << "Loading Shaders... ";
    loadShaders();
    std::cerr << "Done" << std::endl;

    for (auto &shader : shaders_)
        setupShader(shader);

    // Set the light based uniforms and draw shadows
    setSceneShaderUniforms(scene);
    setupComputeShaders(processed);

    if (graphicsSettings_.shadowResolution)
        renderShadowMaps(scene, true);

    // Assign the skybox to any ocean shaders
    borrowSkybox(drawBuckets_, bucketHandles);

    // Dynamic cubemap generation. The scene must be ready to render, except water / ocean.
    createCubemaps(scene, drawBuckets_);

    // Update the GPU metadata buffers for the freshly created cubemaps
    updateMetadataCubemaps(scene, metadataBridge);

    // Get ready for first frame
    bindBufferRanges(0);
    updateCameraUBO(scene.getCamera());

    // Unbind for Hi-Z at start of frame and prime texture so everything passes
    frameBuffer_.unbind();
    GLfloat one{1.0f};
    for (std::size_t i{0}; i < hiZViews_.getMipViews().size(); ++i)
        glClearTexImage(hiZTexture_, static_cast<GLint>(i), GL_RED, GL_FLOAT, &one);
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
    matricesBaseIndices_ = std::move(processed.bufferDescriptors.matricesBaseIndexMap);
    boneTransformsBaseIndexMap_ = std::move(processed.bufferDescriptors.boneTransformsBaseIndexMap);

    drawCommandsSize_ = static_cast<std::uint32_t>(processed.bufferDescriptors.drawCommandsSize);
    cullResultsSize_ = static_cast<std::uint32_t>(processed.bufferDescriptors.cullResultsSize);
    drawCountsSize_ = static_cast<std::uint32_t>(processed.bufferDescriptors.drawCountsSize);
    visibleInstancesSize_ = static_cast<std::uint32_t>(processed.bufferDescriptors.visibleInstancesSize);
}

void Renderer::setupComputeShaders(const SceneProcessor::ProcessedScene &processed)
{
    cullWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptors.numInstances);
    compactionWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptors.numPrimitives);
    particleWorkGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptors.numParticles);
    skinnedVertexGroupSize_ = static_cast<std::uint32_t>(processed.sceneDescriptors.numSkinnedVertices);

    std::uint32_t baseVBOIndex{static_cast<std::uint32_t>(processed.bufferDescriptors.particleBaseVBOIndex)};

    particleShader_.setIntU("vboBaseIndex", baseVBOIndex);
    cullShader_.setInt("screenWidth", graphicsSettings_.renderWidth);
    cullShader_.setInt("screenHeight", graphicsSettings_.renderHeight);
    msaaDepthResolveShader_.setInt("resReduction", hiZResolutionReduction);
    msaaDepthResolveShader_.setIVec2("mipRes", glm::ivec2{graphicsSettings_.renderWidth / hiZResolutionReduction,
                                                          graphicsSettings_.renderHeight / hiZResolutionReduction});
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    const DirectionalLight &light{scene.getGlobalLight()};

    const Camera &camera{scene.getCamera()};
    const float fov{camera.getFOV()};
    const float nearPlane{camera.getNearPlane()};

    for (auto &shader : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", light.getColor());
        shader.setVec3("lightDir", light.getDirection());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());

        shader.setFloat("fov", fov);
    }

    cullShader_.setFloat("nearPlane", nearPlane);
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

    // 90 degrees for cubemap
    Camera camera;
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
    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType != water)
            continue;

        const Transform &transform{entry.worldTransforms.front()};

        // Set the camera in the center of the water plane
        camera.setPosition(transform.getPosition());
        renderCubeFaces(camera, cubeFaces, blitTarget, parameterBufferIndex);

        // Assign the texture
        std::vector<int> materialIndices;
        materialIndices.reserve(entry.primitives.size());

        for (const Primitive &primitive : entry.primitives)
            materialIndices.push_back(primitive.materialIndex);

        textureManager_.setCubemap(entry.handle, materialIndices, blitTarget);
    }

    cubeFaces.unbind();
    glViewport(0, 0, graphicsSettings_.renderWidth, graphicsSettings_.renderHeight);
}

void Renderer::renderCubeFaces(Camera &camera, RenderTarget &target, RenderTarget &blitTarget,
                               std::size_t parameterBufferIndex)
{
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    auto renderFace = [&](WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateCameraUBO(camera);

        // Use buffer section 0 since its a main render pass. Frustum only since Hi-Z is not prepped
        cullScene(cullFrustumShader_, camera.getViewProjectionMatrix(), 0);

        GLuint zero{0};
        glNamedBufferSubData(SSBOs_.drawCounts, static_cast<GLsizeiptr>(parameterBufferIndex * sizeof(std::uint32_t)),
                             sizeof(GLuint), &zero);

        renderScene();
    };

    for (int i{0}; i < 6; ++i)
    {
        target.setTextureLayer(i);
        blitTarget.setTextureLayer(i);

        target.clear();
        renderFace(faces[i], camera);

        target.blitTo(blitTarget);
    }
}

void Renderer::updateMetadataCubemaps(Scene &scene, const std::vector<std::uint32_t> &metadataBridge)
{
    // Verify data layout before sparse updates of the metadata struct
    static_assert(std::is_standard_layout_v<SamplerArrayIndex>);

    std::size_t primitiveOffset{0};

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        const std::size_t numPrimitives{entry.primitives.size()};

        if (entry.shaderType != ocean && entry.shaderType != water)
        {
            primitiveOffset += numPrimitives;
            continue;
        }

        std::span<const Material> materials{textureManager_.getMaterials(entry.handle)};

        for (std::size_t i{0}; i < entry.primitives.size(); ++i)
        {
            const Material &material{materials[entry.primitives[i].materialIndex]};
            const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};

            const std::uint32_t bufferIndex{metadataBridge[primitiveOffset + i]};
            const std::size_t bufferOffset{offsetof(PrimitiveMetadata, cubemap)};

            glNamedBufferSubData(SSBOs_.metadata,
                                 static_cast<GLintptr>(bufferIndex * sizeof(PrimitiveMetadata) + bufferOffset),
                                 sizeof(SamplerArrayIndex), &cubemap);
        }

        primitiveOffset += numPrimitives;
    }
}