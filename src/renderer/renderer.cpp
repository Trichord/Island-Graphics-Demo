#include "renderer/renderer.h"

#include <algorithm>
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
#include "models/primitive.h"
#include "renderer/cascade_shadow.h"
#include "renderer/render_target.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/compute_shader.h"
#include "renderer/shader/graphics_shader.h"
#include "renderer/shader/shader_base.h"
#include "renderer/textures.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/texture_utilities.h"
#include "utilities/transform.h"

namespace
{
// Hardware shadow bias
constexpr GLfloat polygonFactor{2.0f};
constexpr GLfloat polygonUnits{2.0f};
constexpr GLfloat polygonClamp{0.02f};

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

// Hierarchical Z-Buffer Occlusion Culling
constexpr Sampler hiZSampler{
    .magFilter = GL_NEAREST,
    .minFilter = GL_NEAREST_MIPMAP_NEAREST,
    .wrapS = GL_CLAMP_TO_EDGE,
    .wrapT = GL_CLAMP_TO_EDGE,
};

constexpr GLenum hiZInternalFormat{GL_R32F};

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

Renderer::Renderer(TextureManager &textureManager, const GraphicsSettings &settings, int windowWidth, int windowHeight)
    : graphicsSettings_{settings}, textureManager_{textureManager}, externalWidth_{windowWidth},
      externalHeight_{windowHeight},
      frameBuffer_{
          settings.renderWidth,
          settings.renderHeight,
          {.msaa = settings.msaa,
           .depthType = RenderTarget::DepthType::texture,
           .internalFormat = GL_RGBA16F,
           .depthFormat = GL_DEPTH_COMPONENT24,
           .mipmaps = true},
      },
      msaaResolveTarget_{settings.renderWidth, settings.renderHeight, {.internalFormat = GL_RGBA16F}},
      shadowPassTarget_{settings.shadowResolution ? RenderTarget{settings.shadowResolution,
                                                                 settings.shadowResolution,
                                                                 {
                                                                     .depthType = RenderTarget::DepthType::texture,
                                                                     .depthFormat = GL_DEPTH_COMPONENT24,
                                                                     .textureArray = true,
                                                                     .textureArrayDepth = shadowCascadeCount,
                                                                     .shadowMap = true,
                                                                 }}
                                                  : RenderTarget{}},
      hiZTexture_{texture::createTextureID(settings.renderWidth / hiZResolutionReduction,
                                           settings.renderHeight / hiZResolutionReduction, hiZInternalFormat,
                                           hiZSampler, true, false)},
      noiseTexture_{texture::textureFromFile("./res/textures/perlin.ktx2", {})},
      hiZViews_{hiZTexture_, hiZSampler, hiZInternalFormat,
                texture::calculateMipmapLevels(settings.renderWidth / hiZResolutionReduction,
                                               settings.renderHeight / hiZResolutionReduction, false)}
{
    glState_.bindTexture(static_cast<GLuint>(TextureUnit::depthMap), frameBuffer_.getDepthMapTextureID());
    glState_.bindTexture(static_cast<GLuint>(TextureUnit::resolvedColor), msaaResolveTarget_.getColorTextureID());
    glState_.bindTexture(static_cast<GLuint>(TextureUnit::shadow), shadowPassTarget_.getDepthMapTextureID());
    glState_.bindTexture(static_cast<GLuint>(TextureUnit::hiZ), hiZTexture_);
    glState_.bindTexture(static_cast<GLuint>(TextureUnit::noise), noiseTexture_);
    glState_.bindFramebuffer(frameBuffer_.getID());
    glPolygonOffsetClamp(polygonFactor, polygonUnits, polygonClamp);
}

Renderer::~Renderer()
{
    if (hiZTexture_)
        glDeleteTextures(1, &hiZTexture_);

    if (noiseTexture_)
        glDeleteTextures(1, &noiseTexture_);
}

void Renderer::render(const Scene &scene, const Camera &camera, float time, float deltaTime)
{
    glState_.setDepthWrite(true);

    // Updates from scene first
    updateSceneTransforms(scene);
    updateTimeUBO(time, deltaTime);

    const DirectionalLight &globalLight{scene.getGlobalLight()};
    const bool directionChanged{globalLight.directionHasUpdated()};

    if (directionChanged || globalLight.colorHasUpdated())
        updateSceneUBO(scene);

    updateParticles();
    updateSkinnedVertices();

    if (graphicsSettings_.shadowResolution && (framesRendered_ % 2u || directionChanged))
        renderShadowMaps(scene, camera, directionChanged);

    if (camera.hasUpdated())
        updateCameraUBO(camera);

    glState_.bindFramebuffer(frameBuffer_.getID());
    frameBuffer_.clear();

    const glm::mat4 &cameraVP{camera.getViewProjectionMatrix()};
    cullScene(cullShader_, cameraVP, BufferRange::mainPass);
    renderScene();
    blitFrame();

    // One frame lag on occlusion culling
    cullShader_.setMat4("lastFrameVP", cameraVP);
    updateHiZ();

    ++framesRendered_;
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    const std::vector<std::string> defines{
        graphicsSettings_.shadowResolution ? "ALPHA_TESTED_SHADOWS" : "",
        graphicsSettings_.gamma != 1.0f ? "GAMMA_CORRECT" : "",
        "MSAA_SAMPLES " + std::to_string(graphicsSettings_.msaa),
    };

    graphicsShaders_[shaderIndex(baseColor)] = {"./shaders/base_color.vert", "./shaders/base_color.frag", defines};
    graphicsShaders_[shaderIndex(directionalPBR)] = {"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag",
                                                     defines};
    graphicsShaders_[shaderIndex(grass)] = {"./shaders/grass.vert", "./shaders/grass.frag", defines};
    graphicsShaders_[shaderIndex(trees)] = {"./shaders/trees.vert", "./shaders/trees.frag", defines};
    graphicsShaders_[shaderIndex(reflective)] = {"./shaders/reflective.vert", "./shaders/reflective.frag", defines};
    graphicsShaders_[shaderIndex(skybox)] = {"./shaders/skybox.vert", "./shaders/skybox.frag", defines};
    graphicsShaders_[shaderIndex(water)] = {"./shaders/water.vert", "./shaders/water.frag", defines};
    graphicsShaders_[shaderIndex(ocean)] = {"./shaders/water.vert", "./shaders/ocean.frag", defines};
    graphicsShaders_[shaderIndex(particles)] = {"./shaders/particles.vert", "./shaders/particles.frag", defines};

    // Internal use only shaders
    if (graphicsSettings_.shadowResolution)
    {
        shadowShader_ = {"./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"};
        shadowAlphaShader_ = {"./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag", defines};
    }

    fboResolveShader_ = {"./shaders/fbo_resolve.vert", "./shaders/fbo_resolve.frag", defines};

    // Compute
    cullShader_ = {"./shaders/cull.comp"};
    cullFrustumShader_ = {"./shaders/cull_frustum.comp"};
    compactionShader_ = {"./shaders/compact_draws.comp"};
    particleShader_ = {"./shaders/particles.comp"};
    skinningShader_ = {"./shaders/vertex_skinning.comp"};
    msaaDepthResolveShader_ = {"./shaders/msaa_depth.comp", defines};
    hiZShader_ = {"./shaders/hi_z.comp"};
}

void Renderer::updateCameraUBO(const Camera &camera)
{
    PackedCameraUniforms uniforms{
        .viewMatrix = camera.getViewMatrix(),
        .projectionMatrix = camera.getProjectionMatrix(),
        .viewProjectionMatrix = camera.getViewProjectionMatrix(),
        .inverseViewProjectMatrix = glm::inverse(uniforms.viewProjectionMatrix),
        .cameraPosition = glm::vec4{camera.getPosition(), 0.0f},
        .fov = camera.getFOV(),
    };

    glNamedBufferSubData(UBOs_.camera, 0, static_cast<GLsizeiptr>(sizeof(PackedCameraUniforms)), &uniforms);
}

void Renderer::updateFrustumPlanesUBO(const glm::mat4 &cullMatrix)
{
    const std::array<glm::vec4, 6> planes{getFrustumPlanes(cullMatrix)};
    glNamedBufferSubData(UBOs_.frustumPlanes, 0, static_cast<GLsizeiptr>(6 * sizeof(glm::vec4)), planes.data());
}

void Renderer::updateTimeUBO(float time, float deltaTime)
{
    PackedTime packed{
        .deltaTime = framesRendered_ == 0 ? 0.0f : deltaTime,
        .timeElapsed = time,
    };

    glNamedBufferSubData(UBOs_.time, 0, static_cast<GLsizeiptr>(sizeof(PackedTime)), &packed);
}

void Renderer::updateSceneUBO(const Scene &scene)
{
    const DirectionalLight &light{scene.getGlobalLight()};
    SceneUBO sceneUBO{
        .lightDir = glm::vec4{light.getDirection(), 0.0f},
        .lightColor = glm::vec4{light.getColor(), 1.0f},
        .ambientLight = glm::vec4{scene.getAmbientLight(), 1.0f},
        .windDirection = glm::vec4{scene.getWindDirection(), 1.0f},
        .windStrength = scene.getWindStrength(),
    };

    glNamedBufferSubData(UBOs_.scene, 0, static_cast<GLsizeiptr>(sizeof(SceneUBO)), &sceneUBO);
}

void Renderer::updateWindowUBO()
{
    WindowUBO windowUBO{
        .width = graphicsSettings_.renderWidth,
        .height = graphicsSettings_.renderHeight,
        .gamma = graphicsSettings_.gamma,
    };

    glNamedBufferSubData(UBOs_.window, 0, static_cast<GLsizeiptr>(sizeof(WindowUBO)), &windowUBO);
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
        cascadeShadows.nearPlane = {cascades[0].near, cascades[1].near, cascades[2].near, 0.0f};
        cascadeShadows.farPlane = {cascades[0].far, cascades[1].far, cascades[2].far, 0.0f};
        bytes += sizeof(cascadeShadows.farPlane);
        bytes += sizeof(cascadeShadows.nearPlane);
    }

    int cascadesCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    for (int i{0}; i < cascadesCount; ++i)
    {
        cascadeShadows.matrices[i] = cascades[i].viewProjection;
        bytes += sizeof(cascadeShadows.matrices[i]);
    }

    glNamedBufferSubData(UBOs_.cascadeShadows, offset, bytes, data);
}

void Renderer::updateSceneTransforms(const Scene &scene)
{
    for (const Scene::Entry &entry : scene.getUpdatedEntries())
    {
        const std::vector<std::uint32_t> &bufferIndices{matricesBaseIndices_[entry.handle]};
        auto boneIter = boneTransformsBaseIndexMap_.find(entry.handle);
        const std::size_t numTransforms{entry.worldTransforms.size()};

        updatedMatrices_.reserve(numTransforms);

        for (std::size_t i{0}; i < entry.primitives.size(); ++i)
        {
            updatedMatrices_.clear();

            const Primitive &primitive{entry.primitives[i]};

            for (const Transform &transform : entry.worldTransforms)
                updatedMatrices_.emplace_back(transform.getModelMatrix() * primitive.modelSpaceTransform);

            glNamedBufferSubData(SSBOs_.matrices, static_cast<GLintptr>(bufferIndices[i] * sizeof(glm::mat4)),
                                 static_cast<GLsizeiptr>(numTransforms * sizeof(glm::mat4)), updatedMatrices_.data());
        }

        if (boneIter == boneTransformsBaseIndexMap_.end())
            continue;

        std::uint32_t boneTransformsOffset{boneIter->second};
        const std::vector<glm::mat4> &jointMatrices{entry.entryAnimation->getJointMatrices()};

        glNamedBufferSubData(SSBOs_.boneTransforms, static_cast<GLintptr>(boneTransformsOffset * sizeof(glm::mat4)),
                             static_cast<GLsizeiptr>(jointMatrices.size() * sizeof(glm::mat4)), jointMatrices.data());
    }
}

void Renderer::updateCascades(const Scene &scene, const Camera &camera, bool lightChanged)
{
    // Move the far plane closer for tighter cascades
    const glm::mat4 cameraVP{camera.getResizedViewProjectionMatrix(farPlaneReduction)};
    float farPlane{camera.getFarPlane() * farPlaneReduction};

    // Recalculate frustum splits and light VP matrices
    cascadeShadow_.updateCascadeShadows(cameraVP, scene.getGlobalLight(), graphicsSettings_.shadowResolution, farPlane);

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
void Renderer::cullScene(ComputeShader &cullShader, const glm::mat4 &cullMatrix, BufferRange range)
{
    constexpr std::uint32_t shaderLocalSize{64u};

    // Must bind the range here, otherwise the culling and compaction shaders will happily write into the wrong offset
    bindBufferRanges(range);

    const GLintptr drawCountOffset{static_cast<GLintptr>(bufferRangeIndex(range)) *
                                   static_cast<GLintptr>(drawCountsSize_)};
    const GLintptr cullResultsOffset{static_cast<GLintptr>(bufferRangeIndex(range)) *
                                     static_cast<GLintptr>(cullResultsSize_)};

    glClearNamedBufferSubData(SSBOs_.drawCounts, GL_R32UI, drawCountOffset, static_cast<GLsizeiptr>(drawCountsSize_),
                              GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferSubData(SSBOs_.cullResults, GL_R32UI, cullResultsOffset,
                              static_cast<GLsizeiptr>(cullResultsSize_), GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    updateFrustumPlanesUBO(cullMatrix);

    glState_.bindProgram(cullShader);
    cullShader.dispatch((cullWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glState_.bindProgram(compactionShader_);
    compactionShader_.dispatch((compactionWorkGroupSize_ + (shaderLocalSize - 1)) / shaderLocalSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

void Renderer::updateParticles()
{
    constexpr std::uint32_t localSizeX{64u};

    if (!particleWorkGroupSize_)
        return;

    glState_.bindProgram(particleShader_);
    particleShader_.dispatch((particleWorkGroupSize_ + (localSizeX - 1)) / localSizeX, 1, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void Renderer::updateSkinnedVertices()
{
    constexpr std::uint32_t localSizeX{64u};

    if (!skinnedVertexGroupSize_)
        return;

    glState_.bindProgram(skinningShader_);
    skinningShader_.dispatch((skinnedVertexGroupSize_ + (localSizeX - 1)) / localSizeX, 1, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void Renderer::updateHiZ()
{
    constexpr std::uint32_t localSizeX{8u};
    constexpr std::uint32_t localSizeY{8u};
    constexpr std::uint32_t one{1u};

    const std::vector<GLuint> &mipViews{hiZViews_.getMipViews()};

    if (mipViews.empty())
        return;

    // Resolve msaa depth to the first mip level to simplify the Hi-Z pyramid building
    glState_.bindProgram(msaaDepthResolveShader_);

    glBindImageTexture(static_cast<GLuint>(ImageUnit::hiZWrite), mipViews[0], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       hiZInternalFormat);

    std::uint32_t textureWidth{static_cast<std::uint32_t>(graphicsSettings_.renderWidth / hiZResolutionReduction)};
    std::uint32_t textureHeight{static_cast<std::uint32_t>(graphicsSettings_.renderHeight / hiZResolutionReduction)};

    msaaDepthResolveShader_.dispatch((textureWidth + (localSizeX - 1)) / localSizeX,
                                     (textureHeight + (localSizeY - 1)) / localSizeY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glState_.bindProgram(hiZShader_);

    for (std::size_t i{1}; i < mipViews.size(); ++i)
    {
        std::uint32_t srcMipX{std::max(one, textureWidth >> (i - 1))};
        std::uint32_t srcMipY{std::max(one, textureHeight >> (i - 1))};
        std::uint32_t dstMipX{std::max(one, textureWidth >> i)};
        std::uint32_t dstMipY{std::max(one, textureHeight >> i)};

        glBindImageTexture(static_cast<GLuint>(ImageUnit::hiZRead), mipViews[i - 1], 0, GL_FALSE, 0, GL_READ_ONLY,
                           hiZInternalFormat);
        glBindImageTexture(static_cast<GLuint>(ImageUnit::hiZWrite), mipViews[i], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                           hiZInternalFormat);

        hiZShader_.setIVec4("mipSizes", glm::ivec4{srcMipX, srcMipY, dstMipX, dstMipY});
        hiZShader_.dispatch((dstMipX + (localSizeX - 1)) / localSizeX, (dstMipY + (localSizeY - 1)) / localSizeY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

// Blits frame from offscreen FBO and leaves the default framebuffer bound for Hi-Z at the start of next frame
void Renderer::blitFrame()
{
    frameBuffer_.blitTo(msaaResolveTarget_);

    const int internalWidth{frameBuffer_.getWidth()};
    const int internalHeight{frameBuffer_.getHeight()};
    const bool changeViewport{internalWidth != externalWidth_ || internalHeight != externalHeight_};

    if (changeViewport)
        glViewport(0, 0, static_cast<GLsizei>(externalWidth_), static_cast<GLsizei>(externalHeight_));

    glState_.bindFramebuffer(0);
    glState_.bindProgram(fboResolveShader_);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (changeViewport)
        glViewport(0, 0, static_cast<GLsizei>(internalWidth), static_cast<GLsizei>(internalHeight));

    glEnable(GL_DEPTH_TEST);
}

void Renderer::updateExternalResolution(int width, int height)
{
    externalWidth_ = width;
    externalHeight_ = height;
}

void Renderer::bindBufferRanges(BufferRange range)
{
    auto bindSSBO = [](BindingPointSSBO bindingPoint, GLuint buffer, std::uint32_t size, BufferRange range) {
        const GLintptr bufferSize{static_cast<GLintptr>(size)};
        const GLintptr offset{bufferSize * static_cast<GLintptr>(bufferRangeIndex(range))};

        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(bindingPoint), buffer, offset, bufferSize);
    };

    using enum BindingPointSSBO;

    bindSSBO(drawCommands, SSBOs_.drawCommands, drawCommandsSize_, range);
    bindSSBO(cullResults, SSBOs_.cullResults, cullResultsSize_, range);
    bindSSBO(drawCounts, SSBOs_.drawCounts, drawCountsSize_, range);
    bindSSBO(visibleInstances, SSBOs_.visibleInstances, visibleInstancesSize_, range);
}