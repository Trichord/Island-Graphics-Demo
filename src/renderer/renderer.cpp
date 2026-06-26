#include "renderer/renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <string>
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
constexpr GLfloat polygonFactor{1.0f};
constexpr GLfloat polygonUnits{1.0f};

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

std::array<glm::vec4, 6> getFrustumPlanes(const glm::mat4 &matrix)
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
} // namespace

Renderer::Renderer(TextureManager &textureManager, const GraphicsSettings &settings)
    : graphicsSettings_{settings}, textureManager_{textureManager},
      frameBuffer_{settings.screenWidth,
                   settings.screenHeight,
                   {.msaa = settings.msaa,
                    .depthType = RenderTarget::DepthType::RBO,
                    .internalFormat = GL_RGBA16F,
                    .depthFormat = GL_DEPTH_COMPONENT24,
                    .mipmaps = true}},
      msaaResolve_{settings.screenWidth, settings.screenHeight, {.internalFormat = GL_RGBA16F}},
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
    glBindTextureUnit(static_cast<GLuint>(TextureUnit::frameBuffer), msaaResolve_.getColorTextureID());
    glBindTextureUnit(static_cast<GLuint>(TextureUnit::shadow), shadowPassTarget_.getDepthMapTextureID());
    glPolygonOffset(polygonFactor, polygonUnits);

    frameBuffer_.bind();
}

void Renderer::renderScene(Scene &scene, float deltaTime)
{
    frameBuffer_.clear();

    updateSceneTransforms(scene);
    updateTimeUBO(deltaTime);
    updateParticles();

    const bool lightChanged{scene.getGlobalLight().directionHasUpdated()};
    updateShaderUniforms(scene, lightChanged);

    if (graphicsSettings_.shadowResolution && (framesRendered_ % 2u || lightChanged))
        renderShadowMaps(scene, lightChanged);

    Camera &camera{scene.getCamera()};

    if (camera.hasUpdated())
        updateCameraUBO(camera);

    cullScene(scene.getCamera().getViewProjectionMatrix(), 0);
    renderScene();
    blitFrame();

    ++framesRendered_;
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    const std::vector<std::string> defines{graphicsSettings_.shadowResolution ? "ALPHA_TESTED_SHADOWS" : "",
                                           !boneTransformsBaseIndexMap_.empty() ? "SKINNED" : "",
                                           graphicsSettings_.gamma != 1.0f ? "GAMMA_CORRECT" : ""};

    shaders_[shaderIndex(directionalPBR)] = {directionalPBR, "./shaders/directional_PBR.vert",
                                             "./shaders/directional_PBR.frag", defines};
    shaders_[shaderIndex(grass)] = {grass, "./shaders/grass.vert", "./shaders/grass.frag", defines};
    shaders_[shaderIndex(trees)] = {trees, "./shaders/trees.vert", "./shaders/trees.frag", defines};
    shaders_[shaderIndex(reflective)] = {reflective, "./shaders/reflective.vert", "./shaders/reflective.frag", defines};
    shaders_[shaderIndex(skybox)] = {skybox, "./shaders/skybox.vert", "./shaders/skybox.frag", defines};
    shaders_[shaderIndex(water)] = {water, "./shaders/water.vert", "./shaders/water.frag", defines};
    shaders_[shaderIndex(ocean)] = {ocean, "./shaders/water.vert", "./shaders/ocean.frag", defines};
    shaders_[shaderIndex(particles)] = {particles, "./shaders/particles.vert", "./shaders/particles.frag", defines};

    // Internal use only shaders
    if (graphicsSettings_.shadowResolution)
    {
        shaders_[shaderIndex(shadowMap)] = {shadowMap, "./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"};
        shaders_[shaderIndex(shadowMapAlpha)] = {shadowMapAlpha, "./shaders/shadow_pass.vert",
                                                 "./shaders/shadow_pass.frag", defines};
    }

    shaders_[shaderIndex(fboResolve)] = {fboResolve, "./shaders/fbo_resolve.vert", "./shaders/fbo_resolve.frag",
                                         defines};

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

void Renderer::updateCameraUBO(Camera &camera)
{
    PackedCameraUniforms uniforms;

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
    std::unordered_set<Handle> &updatedTransformHandles{scene.getUpdatedHandles()};

    for (Handle handle : updatedTransformHandles)
    {
        const std::vector<std::uint32_t> &bufferIndices{matricesBaseIndices_[handle]};
        const Scene::Entry &sceneEntry{scene.getEntryByHandle(handle)};
        const std::vector<Transform> &worldTransforms{sceneEntry.worldTransforms};

        updatedMatrices_.reserve(worldTransforms.size());

        for (std::size_t i{0}; i < sceneEntry.primitives.size(); ++i)
        {
            updatedMatrices_.clear();

            for (const Transform &transform : worldTransforms)
                updatedMatrices_.emplace_back(transform.getModelMatrix() *
                                              sceneEntry.primitives[i].modelSpaceTransform);

            glNamedBufferSubData(SSBOs_.matrices, static_cast<GLintptr>(bufferIndices[i] * sizeof(glm::mat4)),
                                 static_cast<GLsizeiptr>(worldTransforms.size() * sizeof(glm::mat4)),
                                 updatedMatrices_.data());
        }

        auto boneIter = boneTransformsBaseIndexMap_.find(handle);

        if (boneIter == boneTransformsBaseIndexMap_.end())
            continue;

        std::uint32_t boneTransformsOffset{boneIter->second};
        const std::vector<glm::mat4> &jointMatrices{sceneEntry.entryAnimation->animator.getJointMatrices()};

        glNamedBufferSubData(SSBOs_.boneTransforms, static_cast<GLintptr>(boneTransformsOffset * sizeof(glm::mat4)),
                             static_cast<GLsizeiptr>(jointMatrices.size() * sizeof(glm::mat4)), jointMatrices.data());
    }

    updatedTransformHandles.clear();
}

void Renderer::updateCascades(Scene &scene, bool lightChanged)
{
    Camera &camera{scene.getCamera()};
    DirectionalLight &light{scene.getGlobalLight()};

    // Move the far plane closer for tighter cascades
    const glm::mat4 cameraVP{camera.getResizedViewProjectionMatrix(farPlaneReduction)};
    float farPlane{camera.getFarPlane() * farPlaneReduction};

    // Recalculate frustum splits and light VP matrices
    cascadeShadow_.updateCascadeShadows(cameraVP, light, graphicsSettings_.shadowResolution, farPlane);

    // Splits never change, only set on renderer initialization
    const bool updateFullUBO{framesRendered_ == 0};
    updateCascadeShadowUBO(cascadeShadow_.getCascades(), lightChanged, updateFullUBO);
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}

// Culls a scene agains the cull matrix, building a new compacted list of draw commands and updating the parameter
// buffer. Changes active shader program.
void Renderer::cullScene(const glm::mat4 &cullMatrix, int bufferSection)
{
    constexpr int shaderLocalSize{64};

    glClearNamedBufferSubData(SSBOs_.drawCounts, GL_R32UI, static_cast<GLintptr>(bufferSection * drawCountsSize_),
                              static_cast<GLsizeiptr>(drawCountsSize_), GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferSubData(SSBOs_.cullResults, GL_R32UI, static_cast<GLintptr>(bufferSection * cullResultsSize_),
                              static_cast<GLsizeiptr>(cullResultsSize_), GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    updateFrustumPlanesUBO(cullMatrix);

    glState_.bindProgram(cullShader_);
    cullShader_.dispatch((cullWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glState_.bindProgram(compactionShader_);
    compactionShader_.dispatch((compactionWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void Renderer::updateParticles()
{
    constexpr int shaderLocalSize{64};

    if (!particleWorkGroupSize_)
        return;

    glState_.bindProgram(particleShader_);

    int particleCount{static_cast<int>(particleWorkGroupSize_)};
    particleShader_.dispatch((particleCount + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void Renderer::blitFrame()
{
    frameBuffer_.blitTo(msaaResolve_);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

    shaders_[shaderIndex(ShaderType::fboResolve)].use();

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    frameBuffer_.bind();
}