#include "renderer.h"

#include <array>

#include "camera.h"
#include "cascade_shadow.h"
#include "directional_light.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "renderable.h"
#include "scene.h"
#include "settings_window.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "textures.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.111f};
constexpr float alphaCutoff{0.425f};
constexpr int shadowMapMSAA{2};

Renderer::Renderer(const GraphicsSettings &settings)
    : threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)}, textureManager_{threadPool_, settings},
      graphicsSettings_{settings}, shadowPassTarget_{settings.shadowResolution,
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

    renderAllSceneObjects();
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
    shaders_.emplace(SHADOW_MAP_ALPHA, Shader{"./shaders/shadow_pass_alpha.vert", "./shaders/shadow_pass_alpha.frag"});

    if (graphicsSettings_.shadowResolution > 1)
        shaders_.emplace(DIRECTIONAL_PBR,
                         Shader{"./shaders/directional_PBR_shadow.vert", "./shaders/directional_PBR_shadow.frag"});
    else
        shaders_.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
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
    // Set all scene dependent uniforms
    setSceneShaderUniforms(scene);

    // Instanced data must be loaded first, shader-related functions can be called
    loadInstancedData(scene);
    buildShaderMap(scene);
    textureManager_.buildArrayTextures();

    // Next all buffers must be built
    createBuffers(scene);
    createVAO(scene);

    // Skyboxes have to be borrowed before creating cubemaps. The environement must be fully setup before rendering
    // cubemap faces. Cubemap face generation relies on default full visibility, before any culling has happened
    borrowSkybox();
    createCubemaps(scene);

    // Setup for cascade shadow maps. Must come after cubemaps due to per-light-frustum culling
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

    glDeleteBuffers(1, &indicesSSBO_);
    glDeleteBuffers(1, &matricesSSBO_);
}

void Renderer::updateSceneTransforms(Scene &scene)
{
    std::unordered_set<ModelHandle> &updatedTransformHandles{scene.getTransformsUpdated()};

    for (ModelHandle handle : updatedTransformHandles)
    {
        const GLuint baseIndex{matricesBufferIndexByHandle.at(handle)};
        std::vector<BoundedTransform> &instances{scene.getEntryByHandle(handle).transforms};

        std::vector<glm::mat4> matrices;
        matrices.reserve(instances.size());

        for (auto &instance : instances)
            matrices.emplace_back(instance.transform.getModelMatrix());

        glNamedBufferSubData(matricesSSBO_, baseIndex * sizeof(glm::mat4), sizeof(glm::mat4) * matrices.size(),
                             matrices.data());
    }

    updatedTransformHandles.clear();
}

std::uint32_t Renderer::getFrameCount() const
{
    return framesRendered_;
}

// Build up all scene entries by shader type
void Renderer::buildShaderMap(Scene &scene)
{
    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        drawEntriesByShader_[entry.shaderType].push_back({.renderableRef{entry.renderableRef},
                                                          .transformsRef{entry.transforms},
                                                          .shaderType{entry.shaderType},
                                                          .cullMethod{entry.cullMethod}});
    }
}