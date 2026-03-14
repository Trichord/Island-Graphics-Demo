#include "renderer.h"

#include <array>

#include "camera.h"
#include "config_window.h"
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

Renderer::Renderer(const ConfigWindow::GraphicsSettings &settings)
    : textureManager_{threadPool_}, graphicsSettings_{settings},
      shadowPassTarget{2048, 2048, settings.msaa, GL_RG32F, GL_DEPTH_COMPONENT32F, FBODepth::RBO},
      shadowBlitTarget{2048, 2048, 0, GL_RG32F, GL_NONE, FBODepth::NONE}
{
    setupTextureManager();
    loadShaders();

    for (auto &[_, shader] : shaders_)
        setupShader(shader);
}

Renderer::~Renderer()
{
    deleteBuffers();
}

void Renderer::renderScene(Scene &scene)
{
    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor_.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear_);

    if (scene.getGlobalLightSource().directionHasUpdated())
    {
        updateShadowMaps(scene);
        updateShaderUniforms(scene, true);
    }
    else
        updateShaderUniforms(scene, false);

    if (scene.getCamera().hasUpdated())
    {
        updateUBO(scene);
        resetVisibility(scene);
        cullRenderables(scene);
    }

    renderAllSceneObjects(scene);
}

void Renderer::updateUBO(Scene &scene)
{
    Camera &camera{scene.getCamera()};
    uboCameraData_.viewMatrix = camera.getViewMatrix();
    uboCameraData_.projectionMatrix = camera.getProjectionMatrix();
    uboCameraData_.viewProjectionMatrix = camera.getViewProjectionMatrix();
    uboCameraData_.inverseViewProjectMatrix = glm::inverse(uboCameraData_.viewProjectionMatrix);
    uboCameraData_.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};

    glNamedBufferSubData(UBO_, 0, sizeof(PackedCameraData), &uboCameraData_);
}

void Renderer::setupTextureManager()
{
    if (graphicsSettings_.anisotropicFiltering)
        textureManager_.setAnisoLevel(static_cast<float>(graphicsSettings_.anisotropicFiltering));
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    shaders_.emplace(FLOWERS, Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
    shaders_.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
    shaders_.emplace(POINT_PBR, Shader{"./shaders/point_PBR.vert", "./shaders/point_PBR.frag"});
    shaders_.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
    shaders_.emplace(DIRECTIONAL_PBR_INSTANCED,
                     Shader{"./shaders/directional_PBR_instanced.vert", "./shaders/directional_PBR.frag"});
    shaders_.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert", "./shaders/reflective.frag"});
    shaders_.emplace(SKYBOX, Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
    shaders_.emplace(WATER, Shader{"./shaders/water.vert", "./shaders/water.frag"});
    shaders_.emplace(OCEAN, Shader{"./shaders/water.vert", "./shaders/ocean.frag"});
}

void Renderer::setupShader(Shader &shader)
{
    using enum Texture;

    shader.setUniformBlock("matrices", 0);

    shader.setInt("albedoMap", ALBEDO);
    shader.setInt("armMap", ARM);
    shader.setInt("normalMap", NORMAL);
    shader.setInt("cubeMap", CUBEMAP);
    shader.setInt("depthMap", DEPTH);

    shader.setFloat("nearPlane", cameraDefaultValues::nearPlane);
    shader.setFloat("farPlane", cameraDefaultValues::farPlane);

    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    shader.setFloat("gamma", graphicsSettings_.gamma);
}

void Renderer::updateShaderUniforms(Scene &scene, bool lightDirectionChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLightSource()};

    // Set uniforms
    for (auto &[_, shader] : shaders_)
    {
        using enum ShaderType;

        shader.setFloat("time", (float)glfwGetTime());

        if (lightDirectionChanged)
            shader.setVec3("lightDir", globalLight.getDirection());
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    // Set uniforms
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
    loadInstancedData(scene);
    createBuffers(scene);
    createCubemaps(scene);
    borrowSkyboxes(scene);
    createShadowMaps(scene);
    updateUBO(scene);
    setSceneShaderUniforms(scene);
    cullRenderables(scene);
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager_;
}

void Renderer::deleteBuffers()
{
    glDeleteBuffers(1, &UBO_);
    glDeleteBuffers(1, &matricesSSBO_);
}

void Renderer::allResourcesLoaded()
{
    threadPool_.stopAllThreads();
}

void Renderer::updateShadowMaps(Scene &scene)
{
}