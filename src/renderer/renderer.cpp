#include "renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "camera.h"
#include "cascade_shadow.h"
#include "compute_shader.h"
#include "directional_light.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "handle.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "thread_pool.h"
#include "transform.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

const std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix);

Renderer::Renderer(ThreadPool &threadPool, TextureManager &textureManager, const GraphicsSettings &settings)
    : graphicsSettings_{settings}, threadPool_{threadPool}, textureManager_{textureManager},
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

void Renderer::renderScene(Scene &scene)
{
    ++framesRendered_;

    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor_.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear_);

    const bool lightChanged{scene.getGlobalLight().directionHasUpdated()};
    const bool cameraUpdated{scene.getCamera().hasUpdated()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, lightChanged);

    if (framesRendered_ % 2u && graphicsSettings_.shadowResolution > 1 || lightChanged)
        renderShadowMaps(scene, lightChanged);

    if (cameraUpdated)
        updateCameraUBO(scene);

    cullScene(scene.getCamera().getViewProjectionMatrix());
    renderScene();
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
    // Constants
    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    // Setup Window settings
    shader.setFloat("gamma", graphicsSettings_.gamma);
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

void Renderer::updateShaderUniforms(Scene &scene, bool lightChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLight()};

    for (auto &shader : shaders_)
    {
        if (lightChanged)
            shader.setVec3("lightDir", globalLight.getDirection());
    }
}

void Renderer::updateSceneTransforms(Scene &scene)
{
    std::unordered_set<Handle> &updatedTransformHandles{scene.getUpdatedModels()};

    for (Handle handle : updatedTransformHandles)
    {
        BufferTransforms &bufferTransforms{transformsByHandle_.at(handle)};
        std::size_t numMatrices{bufferTransforms.transforms->size()};

        updatedMatrices_.clear();
        updatedMatrices_.reserve(numMatrices);

        for (Transform &transform : *bufferTransforms.transforms)
            updatedMatrices_.emplace_back(transform.getModelMatrix());

        glNamedBufferSubData(matricesBuffer_, bufferTransforms.matricesBufferBaseIndex * sizeof(glm::mat4),
                             numMatrices * sizeof(glm::mat4), updatedMatrices_.data());
    }

    updatedTransformHandles.clear();
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager_;
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

const std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix)
{
    auto normalizePlane = [](glm::vec4 &plane) {
        float length{glm::length(glm::vec3{plane})};
        plane /= length;
    };

    const glm::vec4 row0{glm::row(matrix, 0)};
    const glm::vec4 row1{glm::row(matrix, 1)};
    const glm::vec4 row2{glm::row(matrix, 2)};
    const glm::vec4 row3{glm::row(matrix, 3)};

    glm::vec4 left{row3 + row0};
    glm::vec4 right{row3 - row0};
    glm::vec4 top{row3 - row1};
    glm::vec4 bottom{row3 + row1};
    glm::vec4 near{row3 + row2};
    glm::vec4 far{row3 - row2};

    normalizePlane(left);
    normalizePlane(right);
    normalizePlane(top);
    normalizePlane(bottom);
    normalizePlane(near);
    normalizePlane(far);

    return {left, right, top, bottom, near, far};
}

Renderer::~Renderer()
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