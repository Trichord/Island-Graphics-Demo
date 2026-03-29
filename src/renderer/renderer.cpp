#include "renderer.h"

#include <array>

#include "camera.h"
#include "cascade_shadow.h"
#include "settings_window.h"
#include "directional_light.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_units.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

Renderer::Renderer(const GraphicsSettings &settings)
    : textureManager_{threadPool_, settings}, graphicsSettings_{settings},
      shadowPassTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {
                            shadowMapMSAA,
                            RenderTarget::DepthType::RBO,
                            GL_RG32F,
                            GL_DEPTH_COMPONENT32F,
                        }},
      shadowBlitTarget_{settings.shadowResolution,
                        settings.shadowResolution,
                        {0, RenderTarget::DepthType::NONE, GL_RG32F, GL_NONE, true, CascadeShadow::cascadeCount}}
{
    loadShaders();

    for (auto &[_, shader] : shaders_)
        setupShader(shader);

    glBindTextureUnit(static_cast<GLuint>(Texture::SHADOW), shadowBlitTarget_.getColorTextureID());
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
    const bool sceneUpdated{!scene.getTransformsUpdated().empty()};
    const bool cameraUpdated{scene.getCamera().hasUpdated()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, globalLightUpdated);

    if (framesRendered_ % 2u && graphicsSettings_.shadowResolution > 1)
        updateShadowMaps(scene);

    if (cameraUpdated)
    {
        updateCameraUBO(scene);
        cullRenderables(scene, scene.getCamera().getViewProjectionMatrix());
    }

    renderAllSceneObjects(scene);
}

void Renderer::updateCameraUBO(Scene &scene)
{
    Camera &camera{scene.getCamera()};
    packedCameraData_.viewMatrix = camera.getViewMatrix();
    packedCameraData_.projectionMatrix = camera.getProjectionMatrix();
    packedCameraData_.viewProjectionMatrix = camera.getViewProjectionMatrix();
    packedCameraData_.inverseViewProjectMatrix = glm::inverse(packedCameraData_.viewProjectionMatrix);
    packedCameraData_.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};

    glNamedBufferSubData(cameraUBO_, 0, sizeof(PackedCameraData), &packedCameraData_);
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    shaders_.emplace(GRASS, Shader{"./shaders/grass.vert", "./shaders/grass.frag"});
    shaders_.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
    shaders_.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert", "./shaders/reflective.frag"});
    shaders_.emplace(SKYBOX, Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
    shaders_.emplace(WATER, Shader{"./shaders/water.vert", "./shaders/water.frag"});
    shaders_.emplace(OCEAN, Shader{"./shaders/water.vert", "./shaders/ocean.frag"});
    shaders_.emplace(SHADOW_MAP, Shader{"./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"});

    if (graphicsSettings_.shadowResolution > 1)
        shaders_.emplace(DIRECTIONAL_PBR,
                         Shader{"./shaders/directional_PBR_shadow.vert", "./shaders/directional_PBR_shadow.frag"});
    else
        shaders_.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
}

void Renderer::setupShader(Shader &shader)
{
    using enum Texture;

    shader.setInt("albedoMap", static_cast<GLuint>(ALBEDO));
    shader.setInt("armMap", static_cast<GLuint>(ARM));
    shader.setInt("normalMap", static_cast<GLuint>(NORMAL));
    shader.setInt("cubeMap", static_cast<GLuint>(CUBEMAP));
    shader.setInt("shadowMap", static_cast<GLuint>(SHADOW));

    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    shader.setFloat("gamma", graphicsSettings_.gamma);
}

void Renderer::updateShaderUniforms(Scene &scene, bool lightDirectionChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLightSource()};

    for (auto &[_, shader] : shaders_)
    {
        shader.setFloat("time", (float)glfwGetTime());

        if (lightDirectionChanged)
            shader.setVec3("lightDir", globalLight.getDirection());
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    for (auto &[_, shader] : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", scene.getGlobalLightSource().getColor());

        shader.setFloat("windStrength", scene.getWindStrength());
        shader.setVec3("windDirection", scene.getWindDirection());
    }
}

void Renderer::initializeResources(Scene &scene)
{
    setSceneShaderUniforms(scene);
    loadInstancedData(scene);
    createBuffers(scene);
    borrowSkyboxes(scene);
    createCubemaps(scene);

    // Setup for cascade shadow maps
    Camera &camera{scene.getCamera()};
    cascadeShadow_ = CascadeShadow(camera.getNearPlane());
    updateShadowMaps(scene);

    updateCameraUBO(scene);
    threadPool_.stopAllThreads();
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager_;
}

void Renderer::deleteBuffers()
{
    glDeleteBuffers(1, &cameraUBO_);
    glDeleteBuffers(1, &shadowCascadesUBO_);

    glDeleteBuffers(1, &indicesSSBO_);
    glDeleteBuffers(1, &matricesSSBO_);
}

void Renderer::updateSceneTransforms(Scene &scene)
{
    std::unordered_set<ModelHandle> &updatedTransformHandles{scene.getTransformsUpdated()};

    for (ModelHandle handle : updatedTransformHandles)
    {
        ssboOffsets &bufferOffset = modelBaseIndices_.at(handle);
        std::span<Instance> updatedTransforms{scene.getInstancesByHandle(handle)};

        std::vector<glm::mat4> matrices;
        matrices.reserve(updatedTransforms.size());

        for (auto &instance : updatedTransforms)
            matrices.emplace_back(instance.transform.getModelMatrix());

        glNamedBufferSubData(matricesSSBO_, bufferOffset.matricesOffset * sizeof(glm::mat4),
                             sizeof(glm::mat4) * matrices.size(), matrices.data());
    }

    updatedTransformHandles.clear();
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}