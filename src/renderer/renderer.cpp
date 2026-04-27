#include "renderer.h"

#include <algorithm>
#include <array>
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
#include "scene.h"
#include "shader.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

Renderer::Renderer(const GraphicsSettings &settings)
    : graphicsSettings_{settings}, threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)},
      textureManager_{threadPool_, settings, getReflectionResolution()},
      shadowPassTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {.msaa = shadowMapMSAA,
                         .depthType = RenderTarget::DepthType::RBO,
                         .internalFormat = GL_RG32F,
                         .depthFormat = GL_DEPTH_COMPONENT32F}},

      shadowBlitTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {.msaa = 0,
                         .depthType = RenderTarget::DepthType::NONE,
                         .internalFormat = GL_RG32F,
                         .depthFormat = GL_NONE,
                         .textureArray = true,
                         .textureArrayDepth = CascadeShadow::cascadeCount}}
{
    loadShaders();

    for (auto &shader : shaders_)
        setupShader(shader);

    glBindTextureUnit(static_cast<GLuint>(TextureUnit::shadow), shadowBlitTarget_.getColorTextureID());
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

    const bool globalLightUpdated{scene.getGlobalLightSource().directionHasUpdated()};
    const bool sceneUpdated{!scene.getUpdatedModels().empty()};
    const bool cameraUpdated{scene.getCamera().hasUpdated()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, globalLightUpdated);

    if (framesRendered_ % 2u && graphicsSettings_.shadowResolution > 1)
        updateShadowMaps(scene);

    if (cameraUpdated)
        updateCameraUBO(scene);

    cullAllModels(scene, scene.getCamera().getViewProjectionMatrix());
    renderAllSceneObjects();
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
    shaders_[shaderIndex(shadowMapAlphaTested)] =
        Shader{shadowMapAlphaTested, "./shaders/shadow_pass_alpha.vert", "./shaders/shadow_pass_alpha.frag"};

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

void Renderer::updateShaderUniforms(Scene &scene, bool lightDirectionChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLightSource()};

    for (auto &shader : shaders_)
    {
        if (lightDirectionChanged)
            shader.setVec3("lightDir", globalLight.getDirection());
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    for (auto &shader : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", scene.getGlobalLightSource().getColor());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());
    }
}

int countDynamicCubeMaps(Scene &scene)
{
    int cubemaps{0};
    for (SceneEntry &entry : scene.getAllSceneEntries())
        if (entry.shaderType == ShaderType::water)
            ++cubemaps;

    return cubemaps;
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

    // Metadata indices must come after the drawEntries are built and sorted, since they map 1:1 to drawEntries.
    assignMetadataIndices();

    // Buffers rely on the renderer's draw list being populated
    std::vector<PrimitiveMetadata> metadata;
    createBuffers(scene, metadata);

    // Skyboxes have to be borrowed before creating cubemaps. The scene must be ready to render, except water / ocean.
    // Cubemap face generation relies on default full visibility, before any culling has happened
    borrowSkybox();
    createCubemaps(scene, reservedArrayEntry);

    // Push the new cubemaps through
    updateMetadataCubemaps(metadata);

    // Setup for cascade shadow maps. Must come after cubemaps due to cascade culling
    cascadeShadow_ = CascadeShadow(scene.getCamera().getNearPlane());
    updateShadowMaps(scene);

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
