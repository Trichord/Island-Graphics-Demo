#include "renderer/renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <unordered_set>
#include <vector>

#include "application/gl_context.h"
#include "application/graphics_settings.h"
#include "renderer/cascade_shadow.h"
#include "renderer/render_target.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/compute_shader.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/thread_pool.h"
#include "utilities/transform.h"

namespace
{
// Hardware shadow bias
constexpr GLfloat polygonFactor{1.11201f};
constexpr GLfloat polygonUnits{1.0f};
} // namespace

const std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix);

Renderer::Renderer(ThreadPool &threadPool, TextureManager &textureManager, const GraphicsSettings &settings)
    : graphicsSettings_{settings}, threadPool_{threadPool}, textureManager_{textureManager},
      shadowPassTarget_{settings.shadowResolution ? RenderTarget{settings.shadowResolution,
                                                                 settings.shadowResolution,
                                                                 {
                                                                     .depthType = RenderTarget::DepthType::texture,
                                                                     .depthFormat = GL_DEPTH_COMPONENT24,
                                                                     .textureArray = true,
                                                                     .textureArrayDepth = shadowCascadeCount,
                                                                     .shadowMap = true,
                                                                 }}
                                                  : RenderTarget{}}
{
    loadShaders();

    for (auto &shader : shaders_)
        setupShader(shader);

    glBindTextureUnit(static_cast<GLuint>(TextureUnit::shadow), shadowPassTarget_.getDepthMapTextureID());
    glPolygonOffset(polygonFactor, polygonUnits);
}

void Renderer::renderScene(Scene &scene, float deltaTime)
{
    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor_.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear_);

    const bool lightChanged{scene.getGlobalLight().directionHasUpdated()};
    const bool cameraUpdated{scene.getCamera().hasUpdated()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, lightChanged);
    updateTimeUBO(deltaTime);
    updateParticles(deltaTime);

    if (graphicsSettings_.shadowResolution && (framesRendered_ % 2u || lightChanged))
        renderShadowMaps(scene, lightChanged);

    if (cameraUpdated)
        updateCameraUBO(scene);

    cullScene(scene.getCamera().getViewProjectionMatrix());
    renderScene();

    ++framesRendered_;
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    if (graphicsSettings_.shadowResolution)
        shaders_[shaderIndex(directionalPBR)] = {directionalPBR, "./shaders/directional_PBR_shadow.vert",
                                                 "./shaders/directional_PBR_shadow.frag"};
    else
        shaders_[shaderIndex(directionalPBR)] = {directionalPBR, "./shaders/directional_PBR.vert",
                                                 "./shaders/directional_PBR.frag"};

    shaders_[shaderIndex(grass)] = {grass, "./shaders/grass.vert", "./shaders/grass.frag"};
    shaders_[shaderIndex(trees)] = {trees, "./shaders/trees.vert", "./shaders/trees.frag"};
    shaders_[shaderIndex(reflective)] = {reflective, "./shaders/reflective.vert", "./shaders/reflective.frag"};
    shaders_[shaderIndex(skybox)] = {skybox, "./shaders/skybox.vert", "./shaders/skybox.frag"};
    shaders_[shaderIndex(water)] = {water, "./shaders/water.vert", "./shaders/water.frag"};
    shaders_[shaderIndex(ocean)] = {ocean, "./shaders/water.vert", "./shaders/ocean.frag"};
    shaders_[shaderIndex(particles)] = {particles, "./shaders/particles.vert", "./shaders/particles.frag"};

    // Internal use only shaders
    shaders_[shaderIndex(shadowMap)] = {shadowMap, "./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"};
    shaders_[shaderIndex(shadowMapAlpha)] = {shadowMapAlpha, "./shaders/shadow_pass_alpha.vert",
                                             "./shaders/shadow_pass_alpha.frag"};

    // Compute
    cullShader_ = {"./shaders/frustum_cull.comp"};
    compactionShader_ = {"./shaders/compact_draws.comp"};
    particleShader_ = {"./shaders/particles.comp"};
}

void Renderer::setupShader(GraphicsShader &shader)
{
    // Setup Window settings
    shader.setFloat("gamma", graphicsSettings_.gamma);
    shader.setInt("screenHeight", graphicsSettings_.screenHeight);
    shader.setInt("screenWidth", graphicsSettings_.screenWidth);
}

void Renderer::updateCameraUBO(Scene &scene)
{
    PackedCameraUniforms uniforms;

    Camera &camera{scene.getCamera()};
    uniforms.viewMatrix = camera.getViewMatrix();
    uniforms.projectionMatrix = camera.getProjectionMatrix();
    uniforms.viewProjectionMatrix = camera.getViewProjectionMatrix();
    uniforms.inverseViewProjectMatrix = glm::inverse(uniforms.viewProjectionMatrix);
    uniforms.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};

    glNamedBufferSubData(UBOs_.camera, 0, static_cast<GLsizeiptr>(sizeof(PackedCameraUniforms)), &uniforms);
}

void Renderer::updateFrustumPlanesUBO(const glm::mat4 &cullMatrix)
{
    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};
    glNamedBufferSubData(UBOs_.frustumPlanes, 0, static_cast<GLsizeiptr>(6 * sizeof(glm::vec4)), planes.data());
}

void Renderer::updateTimeUBO(float deltaTime)
{
    PackedTime time{
        .deltaTime = framesRendered_ == 0 ? 0.0f : deltaTime,
        .timeElapsed = static_cast<float>(glfwGetTime()),
    };

    glNamedBufferSubData(UBOs_.time, 0, static_cast<GLsizeiptr>(sizeof(PackedTime)), &time);
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
        cascadeShadows.near = {cascades[0].near, cascades[1].near, cascades[2].near, 0.0f};
        cascadeShadows.far = {cascades[0].far, cascades[1].far, cascades[2].far, 0.0f};
        bytes += sizeof(cascadeShadows.far);
        bytes += sizeof(cascadeShadows.near);
    }

    int cascadesCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    for (int i{0}; i < cascadesCount; ++i)
    {
        cascadeShadows.matrices[i] = cascades[i].viewProjection;
        bytes += sizeof(cascadeShadows.matrices[i]);
    }

    glNamedBufferSubData(UBOs_.cascadeShadows, offset, bytes, data);
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
        std::uint32_t bufferBaseIndex{matricesBaseIndex_.at(handle)};
        std::vector<Transform> &transforms{scene.getEntryByHandle(handle).transforms};

        updatedMatrices_.clear();
        updatedMatrices_.reserve(transforms.size());

        for (Transform &transform : transforms)
            updatedMatrices_.emplace_back(transform.getModelMatrix());

        glNamedBufferSubData(SSBOs_.matrices, static_cast<GLintptr>(bufferBaseIndex * sizeof(glm::mat4)),
                             static_cast<GLsizeiptr>(transforms.size() * sizeof(glm::mat4)), updatedMatrices_.data());
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
    constexpr int shaderLocalSize{64};

    glClearNamedBufferData(SSBOs_.drawCounts, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferData(SSBOs_.cullResults, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    updateFrustumPlanesUBO(cullMatrix);

    cullShader_.use();
    cullShader_.dispatch((cullWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    compactionShader_.use();
    compactionShader_.dispatch((compactionWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
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

void Renderer::updateParticles(float deltaTime)
{
    constexpr int shaderLocalSize{64};

    if (!particleWorkGroupSize_)
        return;

    particleShader_.use();

    int particleCount{static_cast<int>(particleWorkGroupSize_)};
    particleShader_.dispatch((particleCount + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}