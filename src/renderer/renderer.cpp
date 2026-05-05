#include "renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "fastgltf/types.hpp"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "model.h"
#include "primitive.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

const std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix);

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

    cullScene(scene.getCamera().getViewProjectionMatrix());
    renderScene();
}

void Renderer::updateCameraUBO(Scene &scene)
{
    PackedShaderUniforms uniforms;

    Camera &camera{scene.getCamera()};
    uniforms.viewMatrix = camera.getViewMatrix();
    uniforms.projectionMatrix = camera.getProjectionMatrix();
    uniforms.viewProjectionMatrix = camera.getViewProjectionMatrix();
    uniforms.inverseViewProjectMatrix = glm::inverse(uniforms.viewProjectionMatrix);
    uniforms.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};
    uniforms.time = static_cast<float>(glfwGetTime());

    glNamedBufferSubData(shaderUniformsUBO_, 0, sizeof(PackedShaderUniforms), &uniforms);
}

void Renderer::updateFrustumPlanesUBO(const glm::mat4 &cullMatrix)
{
    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};
    glNamedBufferSubData(frustumPlanesUBO_, 0, 6 * sizeof(glm::vec4), planes.data());
}

void Renderer::updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll)
{
    static_assert(shadowCascadeCount == 3);

    unsigned long long bytes{0};
    PackedCascadeShadows cascadeShadows;

    std::size_t offset{uploadAll ? 0 : offsetof(PackedCascadeShadows, matrices)};
    const void *data{uploadAll ? static_cast<void *>(&cascadeShadows)
                               : static_cast<void *>(&cascadeShadows.matrices[0])};

    if (uploadAll)
    {
        cascadeShadows.splits = {cascades[0].far, cascades[1].far, cascades[2].far, 0.0f};
        bytes += sizeof(cascadeShadows.splits);
    }

    int cascadesCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    for (int i{0}; i < cascadesCount; ++i)
    {
        cascadeShadows.matrices[i] = cascades[i].viewProjection;
        bytes += sizeof(cascadeShadows.matrices[i]);
    }

    glNamedBufferSubData(shadowCascadesUBO_, offset, bytes, data);
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    if (graphicsSettings_.shadowResolution > 1)
        shaders_[shaderIndex(directionalPBR)] = GraphicsShader{directionalPBR, "./shaders/directional_PBR_shadow.vert",
                                                               "./shaders/directional_PBR_shadow.frag"};
    else
        shaders_[shaderIndex(directionalPBR)] =
            GraphicsShader{directionalPBR, "./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"};

    shaders_[shaderIndex(grass)] = GraphicsShader{grass, "./shaders/grass.vert", "./shaders/grass.frag"};
    shaders_[shaderIndex(trees)] = GraphicsShader{trees, "./shaders/trees.vert", "./shaders/trees.frag"};
    shaders_[shaderIndex(reflective)] =
        GraphicsShader{reflective, "./shaders/reflective.vert", "./shaders/reflective.frag"};
    shaders_[shaderIndex(skybox)] = GraphicsShader{skybox, "./shaders/skybox.vert", "./shaders/skybox.frag"};
    shaders_[shaderIndex(water)] = GraphicsShader{water, "./shaders/water.vert", "./shaders/water.frag"};
    shaders_[shaderIndex(ocean)] = GraphicsShader{ocean, "./shaders/water.vert", "./shaders/ocean.frag"};

    // Internal use only shaders
    shaders_[shaderIndex(shadowMap)] =
        GraphicsShader{shadowMap, "./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"};
    shaders_[shaderIndex(shadowMapAlpha)] =
        GraphicsShader{shadowMapAlpha, "./shaders/shadow_pass_alpha.vert", "./shaders/shadow_pass_alpha.frag"};

    // Compute
    cullShader_ = ComputeShader{"./shaders/frustum_cull.comp"};
    compactionShader_ = ComputeShader{"./shaders/compact_draws.comp"};
}

void Renderer::setupShader(GraphicsShader &shader)
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

SceneTotals getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Renderable &renderable{entry.renderableRef.get()};
        std::optional<Handle> borrowed{renderable.getBorrowedHandle()};
        std::span<Primitive> primitives{renderable.getPrimitives()};

        std::size_t numTransforms{entry.transforms.size()};
        std::size_t numPrimitives{primitives.size()};

        if (entry.shaderType == ShaderType::water && !borrowed.has_value())
            ++totals.dynamicCubemaps;

        totals.matrices += numTransforms;
        totals.primitives += numPrimitives;
        totals.visibleInstances += numTransforms * numPrimitives;

        for (Primitive &primitive : primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();
        }
    }

    return totals;
}

AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType == ShaderType::ocean)
            continue;

        Renderable &renderable{entry.renderableRef.get()};
        const AABB &aabb{renderable.getAABB()};

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

std::vector<DrawElementsIndirectCommand> buildDrawCommandBuffer(std::vector<DrawEntry> &entries,
                                                                DrawElementsIndirectCommand &skybox,
                                                                SceneTotals &totals)
{
    std::vector<DrawElementsIndirectCommand> commands;
    commands.reserve(totals.primitives);

    int vaoIndex{0};
    std::uint32_t eboIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        DrawEntry &entry{entries[i]};
        const std::vector<Transform> &transforms{*entry.transforms};
        std::size_t numTransforms{transforms.size()};

        commands.emplace_back(
            DrawElementsIndirectCommand{.count = static_cast<unsigned int>(entry.primitive->indices.size()),
                                        .instanceCount = static_cast<unsigned int>(numTransforms),
                                        .firstIndex = eboIndex,
                                        .baseVertex = vaoIndex,
                                        .baseInstance = static_cast<unsigned int>(i)});

        vaoIndex += static_cast<std::uint32_t>(entry.primitive->vertices.size());
        eboIndex += static_cast<std::uint32_t>(entry.primitive->indices.size());
    }

    return commands;
}

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
    sortDrawEntries(drawEntries);

    // Build draw command buffer for SSBO creation. Make a copy so we can adjust counts
    std::vector<DrawElementsIndirectCommand> commands{buildDrawCommandBuffer(drawEntries, skybox_, totals)};

    createDrawBuckets(drawEntries);

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
    setVisibleInstancesBaseIndex(drawEntries, metadata, totals);
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
    createCubemaps(scene, drawEntries, reservedEntry);

    // Push the new cubemaps through
    updateMetadataCubemaps(drawEntries, metadata);

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
    glDeleteBuffers(1, &mapBuffer_);
    glDeleteBuffers(1, &aabbBuffer_);
    glDeleteBuffers(1, &matricesBuffer_);
    glDeleteBuffers(1, &metadataBuffer_);
    glDeleteBuffers(1, &drawCountsBuffer_);
    glDeleteBuffers(1, &cullResultsBuffer_);
    glDeleteBuffers(1, &drawCommandsBuffer_);
    glDeleteBuffers(1, &visibleInstancesBuffer_);
    glDeleteBuffers(1, &drawCommandsSourceBuffer_);

    glDeleteBuffers(1, &frustumPlanesUBO_);
    glDeleteBuffers(1, &shaderUniformsUBO_);
    glDeleteBuffers(1, &shadowCascadesUBO_);

    glDeleteBuffers(1, &globalEBO_);
    glDeleteBuffers(1, &globalVBO_);
    glDeleteVertexArrays(1, &globalVAO_);
}

// Convert from buffer sub data to a persistant map
void Renderer::updateSceneTransforms(Scene &scene)
{
    std::unordered_set<Handle> &updatedTransformHandles{scene.getUpdatedModels()};

    for (Handle handle : updatedTransformHandles)
    {
        BufferTransforms &transforms{transformsByHandle_.at(handle)};
        std::size_t numMatrices{transforms.transforms->size()};

        updatedMatrices_.clear();
        updatedMatrices_.reserve(numMatrices);

        for (Transform &transform : *transforms.transforms)
            updatedMatrices_.emplace_back(transform.getModelMatrix());

        glNamedBufferSubData(matricesBuffer_, transforms.matricesBufferBaseIndex * sizeof(glm::mat4),
                             numMatrices * sizeof(glm::mat4), updatedMatrices_.data());
    }

    updatedTransformHandles.clear();
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}

// Build up all scene entries by shader type
std::vector<DrawEntry> Renderer::createDrawEntries(Scene &scene, const SceneTotals &totals)
{
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(totals.primitives);

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Renderable &renderable{entry.renderableRef.get()};
        Handle handle{renderable.getHandle()};

        std::span<Primitive> primitives{renderable.getPrimitives()};
        std::uint32_t numTransforms{static_cast<std::uint32_t>(entry.transforms.size())};

        for (Primitive &primitive : primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries.emplace_back(DrawEntry{.handle = handle,
                                               .primitive = &primitive,
                                               .transforms = &entry.transforms,
                                               .shaderType = entry.shaderType,
                                               .alphaMode = primitive.alphaMode});
        }
    }

    return drawEntries;
}

void Renderer::sortDrawEntries(std::vector<DrawEntry> &entries)
{
    std::ranges::sort(entries, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });
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

int Renderer::getReflectionResolution() const
{
    constexpr int resolutionReduction{2};

    return std::max(graphicsSettings_.screenWidth, graphicsSettings_.screenHeight) / resolutionReduction;
}

void Renderer::createDrawBuckets(const std::vector<DrawEntry> &entries)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    ShaderType currentShaderType{entries.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{entries.front().alphaMode};

    std::uint32_t start{0};
    std::uint32_t end{0};

    for (const DrawEntry &entry : entries)
    {
        if (currentShaderType != entry.shaderType || currentAlphaMode != entry.alphaMode)
        {
            drawBuckets_.emplace_back(MultiDrawBucket{.maxCount = end - start,
                                                      .startIndex = start,
                                                      .shaderType = currentShaderType,
                                                      .alphaMode = currentAlphaMode});

            start = end;
            currentShaderType = entry.shaderType;
            currentAlphaMode = entry.alphaMode;
        }

        ++end;
    }

    if (end - start > 0)
        drawBuckets_.emplace_back(MultiDrawBucket{.maxCount = end - start,
                                                  .startIndex = start,
                                                  .shaderType = currentShaderType,
                                                  .alphaMode = currentAlphaMode});
}

std::vector<AABB> Renderer::createAABBs(const std::vector<DrawEntry> &entries, const SceneTotals &totals)
{
    std::vector<AABB> aabbs;
    aabbs.reserve(totals.primitives);

    for (const DrawEntry &entry : entries)
    {
        const AABB &aabb{entry.primitive->aabb};
        aabbs.emplace_back(aabb);
    }

    return aabbs;
}

void Renderer::createBufferMap(const std::vector<DrawEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
                               const SceneTotals &totals)
{
    std::vector<BufferMap> bufferMap;
    bufferMap.reserve(totals.visibleInstances);

    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(entries.size()); ++i)
    {
        std::uint32_t bucket{0};

        for (std::uint32_t j{0}; j < static_cast<std::uint32_t>(drawBuckets_.size() - 1); ++j)
        {
            if (i >= drawBuckets_[j].startIndex && i < drawBuckets_[j + 1].startIndex)
            {
                break;
            }
            else
            {
                ++bucket;
            }
        }

        const std::uint32_t numTransforms{static_cast<std::uint32_t>(entries[i].transforms->size())};
        const std::uint32_t matricesBufferBaseIndex{transformsByHandle_.at(entries[i].handle).matricesBufferBaseIndex};
        const std::uint32_t visibleInstancesBufferBaseIndex{metadata[i].visibleInstancesBufferBaseIndex};

        for (std::uint32_t k{0}; k < numTransforms; ++k)
        {
            bufferMap.emplace_back(BufferMap{
                .matricesIndex = matricesBufferBaseIndex + k,
                .sharedBaseIndex = i,
                .drawBucket = bucket,
                .drawBucketBaseIndex = drawBuckets_[bucket].startIndex,
                .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
            });
        }
    }

    glNamedBufferSubData(mapBuffer_, 0, bufferMap.size() * sizeof(BufferMap), bufferMap.data());
}

const std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix)
{
    const glm::vec4 row0{glm::row(matrix, 0)};
    const glm::vec4 row1{glm::row(matrix, 1)};
    const glm::vec4 row2{glm::row(matrix, 2)};
    const glm::vec4 row3{glm::row(matrix, 3)};

    const glm::vec4 left{glm::normalize(row3 + row0)};
    const glm::vec4 right{glm::normalize(row3 - row0)};
    const glm::vec4 top{glm::normalize(row3 - row1)};
    const glm::vec4 bottom{glm::normalize(row3 + row1)};
    const glm::vec4 near{glm::normalize(row3 + row2)};
    const glm::vec4 far{glm::normalize(row3 - row2)};

    return {left, right, top, bottom, near, far};
}

void Renderer::cullScene(const glm::mat4 &cullMatrix)
{
    glClearNamedBufferData(drawCountsBuffer_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferData(cullResultsBuffer_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    updateFrustumPlanesUBO(cullMatrix);

    cullShader_.use();
    cullShader_.dispatch((cullWorkGroupSize_ + (ComputeShader::localSize - 1u)) / ComputeShader::localSize, 1u, 1u);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    compactionShader_.use();
    compactionShader_.dispatch((compactionWorkGroupSize_ + (ComputeShader::localSize - 1u)) / ComputeShader::localSize,
                               1u, 1u);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}