#include "renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "fastgltf/types.hpp"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "model.h"
#include "primitive.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "shader.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

#include <iostream>

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

Renderer::Renderer(const GraphicsSettings &settings)
    : graphicsSettings_{settings}, threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)},
      textureManager_{threadPool_, settings, getReflectionResolution()},
      msaaShadowTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {.msaa = shadowMapMSAA,
                         .depthType = RenderTarget::DepthType::RBO,
                         .internalFormat = GL_RG32F,
                         .depthFormat = GL_DEPTH_COMPONENT32F}},

      shadowPassTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {.msaa = 0,
                         .depthType = RenderTarget::DepthType::NONE,
                         .internalFormat = GL_RG32F,
                         .depthFormat = GL_NONE,
                         .textureArray = true,
                         .textureArrayDepth = shadowCascadeCount}}
{
    loadShaders();

    for (auto &shader : shaders_)
        setupShader(shader);

    glBindTextureUnit(static_cast<GLuint>(TextureUnit::shadow), shadowPassTarget_.getColorTextureID());
}

Renderer::~Renderer()
{
    deleteBuffers();
}

void Renderer::renderScene(Scene &scene)
{
    ++framesRendered_;

    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor_.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear_);

    const bool lightChanged{scene.getGlobalLight().directionHasUpdated()};
    const bool cameraUpdated{scene.getCamera().hasUpdated()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, lightChanged);

    if (framesRendered_ % 2u && graphicsSettings_.shadowResolution > 1)
        renderShadowMaps(scene, lightChanged);

    if (cameraUpdated)
        updateCameraUBO(scene);

    cullScene(scene, scene.getCamera().getViewProjectionMatrix());
    updateDrawCommandBuffer();
    renderScene();
}

void Renderer::updateCameraUBO(Scene &scene)
{
    Camera &camera{scene.getCamera()};
    packedShaderUniforms_.viewMatrix = camera.getViewMatrix();
    packedShaderUniforms_.projectionMatrix = camera.getProjectionMatrix();
    packedShaderUniforms_.viewProjectionMatrix = camera.getViewProjectionMatrix();
    packedShaderUniforms_.inverseViewProjectMatrix = glm::inverse(packedShaderUniforms_.viewProjectionMatrix);
    packedShaderUniforms_.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};
    packedShaderUniforms_.time = static_cast<float>(glfwGetTime());

    glNamedBufferSubData(cameraUBO_, 0, sizeof(PackedShaderUniforms), &packedShaderUniforms_);
}

void Renderer::updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool initAll)
{
    static_assert(shadowCascadeCount == 3);

    unsigned long long bytes{0};
    std::size_t offset{initAll ? 0 : offsetof(PackedCascadeShadows, matrices)};
    const void *data{initAll ? static_cast<void *>(&packedCascadeShadows_)
                             : static_cast<void *>(&packedCascadeShadows_.matrices[0])};

    if (initAll)
    {
        packedCascadeShadows_.splits = {cascades[0].far, cascades[1].far, cascades[2].far, 0.0f};
        bytes += sizeof(packedCascadeShadows_.splits);
    }

    int cascadesCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    for (int i{0}; i < cascadesCount; ++i)
    {
        packedCascadeShadows_.matrices[i] = cascades[i].viewProjection;
        bytes += sizeof(packedCascadeShadows_.matrices[i]);
    }

    glNamedBufferSubData(shadowCascadesUBO_, offset, bytes, data);
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    shaders_[shaderIndex(grass)] = Shader{grass, "./shaders/grass.vert", "./shaders/grass.frag"};
    shaders_[shaderIndex(trees)] = Shader{trees, "./shaders/trees.vert", "./shaders/trees.frag"};
    shaders_[shaderIndex(reflective)] = Shader{reflective, "./shaders/reflective.vert", "./shaders/reflective.frag"};
    shaders_[shaderIndex(skybox)] = Shader{skybox, "./shaders/skybox.vert", "./shaders/skybox.frag"};
    shaders_[shaderIndex(water)] = Shader{water, "./shaders/water.vert", "./shaders/water.frag"};
    shaders_[shaderIndex(ocean)] = Shader{ocean, "./shaders/water.vert", "./shaders/ocean.frag"};
    shaders_[shaderIndex(shadowMap)] = Shader{shadowMap, "./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"};
    shaders_[shaderIndex(shadowMapAlpha)] =
        Shader{shadowMapAlpha, "./shaders/shadow_pass_alpha.vert", "./shaders/shadow_pass_alpha.frag"};

    if (graphicsSettings_.shadowResolution > 1)
        shaders_[shaderIndex(directionalPBR)] =
            Shader{directionalPBR, "./shaders/directional_PBR_shadow.vert", "./shaders/directional_PBR_shadow.frag"};
    else
        shaders_[shaderIndex(directionalPBR)] =
            Shader{directionalPBR, "./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"};
}

void Renderer::setupShader(Shader &shader)
{
    using enum TextureUnit;

    // Constants
    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    // Setup Window settings
    shader.setFloat("gamma", graphicsSettings_.gamma);
}

void Renderer::updateShaderUniforms(Scene &scene, bool lightChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLight()};

    for (auto &shader : shaders_)
    {
        if (lightChanged)
            shader.setVec3("lightDir", globalLight.getDirection());
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    for (auto &shader : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", scene.getGlobalLight().getColor());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());
    }
}

int countDynamicCubeMaps(Scene &scene)
{
    int cubemaps{0};
    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        const Model &model{entry.modelRef.get()};
        std::optional<ModelHandle> borrowed{model.getBorrowedHandle()};

        if (entry.shaderType == ShaderType::water && !borrowed.has_value())
            ++cubemaps;
    }

    return cubemaps;
}

AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType == ShaderType::ocean)
            continue;

        Model &model{entry.modelRef.get()};
        const AABB &aabb{model.getAABB()};

        for (Transform &transform : entry.transforms)
        {
            const AABB worldAABB{aabb.transform(transform.getModelMatrix())};

            const glm::vec3 aabbMin{worldAABB.getMin()};
            const glm::vec3 aabbMax{worldAABB.getMax()};

            min = glm::min(min, aabbMin);
            max = glm::max(max, aabbMax);
        }
    }

    return {min, max};
}

void Renderer::initializeResources(Scene &scene)
{
    setSceneShaderUniforms(scene);

    // Setup all static scene textures
    textureManager_.finalize2dAllocations();
    textureManager_.build2dTextures();

    SamplerArrayEntry reservedArrayEntry{textureManager_.reserveDynamicCubemaps(
        countDynamicCubeMaps(scene), getReflectionResolution(), textureManager_.getReflectionColorFormat())};

    textureManager_.finalizeCubemapAllocations();
    textureManager_.buildCubemapTextures();

    // Sort the renderer's map by shader & state
    buildDrawEntries(scene);
    sortPrimitives();
    drawCommands_.resize(drawEntries_.size());

    // Metadata indices must come after the drawEntries are built and sorted, since they map 1:1 to drawEntries.
    assignMetadataIndices();

    // Buffers rely on the renderer's draw list being populated
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(drawEntries_.size());
    createBuffers(scene, metadata);
    updateDrawCommandBuffer();

    // Skyboxes have to be borrowed before creating cubemaps. The scene must be ready to render, except water / ocean.
    // Cubemap face generation relies on default full visibility, before any culling has happened
    borrowSkybox();
    createCubemaps(scene, reservedArrayEntry);

    // Push the new cubemaps through
    updateMetadataCubemaps(metadata);

    // Setup for cascade shadow maps. Must come after cubemaps due to cascade culling
    AABB worldAABB{getSceneBounds(scene)};
    float nearPlane{scene.getCamera().getNearPlane()};
    DirectionalLight &light{scene.getGlobalLight()};
    cascadeShadow_ = CascadeShadow(light, worldAABB, nearPlane);

    renderShadowMaps(scene, true, true);

    // Get ready for first frame
    updateCameraUBO(scene);
    threadPool_.stopAllThreads();
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager_;
}

void Renderer::deleteBuffers()
{
    glDeleteVertexArrays(1, &globalVAO_);
    glDeleteBuffers(1, &globalVBO_);
    glDeleteBuffers(1, &globalEBO_);

    glDeleteBuffers(1, &cameraUBO_);
    glDeleteBuffers(1, &shadowCascadesUBO_);

    glDeleteBuffers(1, &visibleIndicesSSBO_);
    glDeleteBuffers(1, &matricesSSBO_);
    glDeleteBuffers(1, &metadataSSBO_);
    glDeleteBuffers(1, &drawCommandBuffer_);
}

// Convert from buffer sub data to a persistant map
void Renderer::updateSceneTransforms(Scene &scene)
{
    std::unordered_set<ModelHandle> &updatedTransformHandles{scene.getUpdatedModels()};

    for (ModelHandle handle : updatedTransformHandles)
    {
        std::vector<std::uint32_t> &drawEntryIndices{drawEntriesByHandle_.at(handle)};

        Model &model{scene.getEntryByHandle(handle).modelRef.get()};

        // Because all primitives of a model share the same underlaying matrices, we can grab the first primitive
        DrawEntry &entry{drawEntries_[drawEntryIndices.front()]};

        std::size_t numMatrices{entry.transforms->size()};

        updatedMatrices_.clear();
        updatedMatrices_.reserve(numMatrices);

        for (Transform &transform : *entry.transforms)
            updatedMatrices_.emplace_back(transform.getModelMatrix());

        glNamedBufferSubData(matricesSSBO_, entry.matricesBufferBaseIndex * sizeof(glm::mat4),
                             numMatrices * sizeof(glm::mat4), updatedMatrices_.data());
    }

    updatedTransformHandles.clear();
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}

// Build up all scene entries by shader type
void Renderer::buildDrawEntries(Scene &scene)
{
    drawEntries_.reserve(scene.getAllSceneEntries().size());

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Model &model{entry.modelRef.get()};
        ModelHandle handle{model.getHandle()};

        std::span<Primitive> primitives{model.getPrimitives()};
        std::uint32_t numTransforms{static_cast<std::uint32_t>(entry.transforms.size())};

        for (Primitive &primitive : primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries_.emplace_back(DrawEntry{.handle = handle,
                                                .primitive = &primitive,
                                                .transforms = &entry.transforms,
                                                .shaderType = entry.shaderType,
                                                .alphaMode = primitive.alphaMode,
                                                .visibleInstances = numTransforms});
        }
    }
}

void Renderer::sortPrimitives()
{
    std::ranges::sort(drawEntries_, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    drawEntriesByHandle_.clear();

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(drawEntries_.size()); ++i)
        drawEntriesByHandle_[drawEntries_[i].handle].push_back(i);
}

void Renderer::assignMetadataIndices()
{
    for (std::uint32_t i{0}; i < static_cast<uint32_t>(drawEntries_.size()); ++i)
        drawEntries_[i].metadataBufferIndex = i;
}

void Renderer::updateMetadataCubemaps(std::vector<PrimitiveMetadata> &metadata)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(drawEntries_.size()); ++i)
    {
        using enum ShaderType;

        if (drawEntries_[i].shaderType == water || drawEntries_[i].shaderType == ocean)
        {
            DrawEntry &drawEntry{drawEntries_[i]};
            PrimitiveMetadata &metaEntry{metadata[i]};

            const int matIndex{drawEntry.primitive->materialIndex};
            const std::vector<Material> &materials{textureManager_.getMaterials(drawEntry.handle)};
            const Material &material{materials[matIndex]};

            metaEntry.cubemapArray = material.cubemap.array;
            metaEntry.cubemapLayer = material.cubemap.layer;

            glNamedBufferSubData(metadataSSBO_, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                                 static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
        }
    }
}

int Renderer::getReflectionResolution() const
{
    constexpr int resolutionReduction{2};

    return std::max(graphicsSettings_.screenWidth, graphicsSettings_.screenHeight) / resolutionReduction;
}

void Renderer::updateDrawCommandBuffer()
{
    drawBuckets_.clear();

    int start{0};
    int end{0};

    ShaderType currentShaderType{drawEntries_.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{drawEntries_.front().alphaMode};

    for (DrawEntry &entry : drawEntries_)
    {
        if (entry.shaderType != currentShaderType || entry.alphaMode != currentAlphaMode)
        {
            drawBuckets_.emplace_back(MultiDrawInfo{.count = end - start,
                                                    .startIndex = start,
                                                    .shaderType = currentShaderType,
                                                    .alphaMode = currentAlphaMode});

            start = end;
            currentShaderType = entry.shaderType;
            currentAlphaMode = entry.alphaMode;
        }

        if (entry.anyVisible)
        {
            drawCommands_[end++] = {.count = static_cast<unsigned int>(entry.primitive->indices.size()),
                                    .instanceCount = entry.visibleInstances,
                                    .firstIndex = entry.eboBaseIndex,
                                    .baseVertex = entry.vaoBaseIndex,
                                    .baseInstance = entry.metadataBufferIndex};
        }
    }

    if (end - start > 0)
        drawBuckets_.emplace_back(MultiDrawInfo{
            .count = end - start,
            .startIndex = start,
            .shaderType = currentShaderType,
            .alphaMode = currentAlphaMode,
        });

    glNamedBufferSubData(drawCommandBuffer_, 0, static_cast<GLsizeiptr>(end * sizeof(DrawElementsIndirectCommand)),
                         drawCommands_.data());
}