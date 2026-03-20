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
    : textureManager_{threadPool_, settings}, graphicsSettings_{settings},
      shadowPassTarget_{shadowMapRes, shadowMapRes, settings.msaa, GL_RG32F, GL_DEPTH_COMPONENT32F, FBODepth::RBO},
      shadowBlitTarget_{shadowMapRes, shadowMapRes, 0, GL_RG32F, GL_NONE, FBODepth::NONE}
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
    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor_.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear_);

    const bool globalLightUpdated{scene.getGlobalLightSource().directionHasUpdated()};
    const bool sceneUpdated{!scene.getTransformsUpdated().empty()};

    updateSceneTransforms(scene);
    updateShaderUniforms(scene, globalLightUpdated);

    if (globalLightUpdated || sceneUpdated)
    {
        updateShadowMaps(scene);
    }

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

void Renderer::loadShaders()
{
    using enum ShaderType;

    shaders_.emplace(FLOWERS, Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
    shaders_.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
    shaders_.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
    shaders_.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert", "./shaders/reflective.frag"});
    shaders_.emplace(SKYBOX, Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
    shaders_.emplace(WATER, Shader{"./shaders/water.vert", "./shaders/water.frag"});
    shaders_.emplace(OCEAN, Shader{"./shaders/water.vert", "./shaders/ocean.frag"});
    shaders_.emplace(SHADOW_MAP, Shader{"./shaders/shadow_pass.vert", "./shaders/shadow_pass.frag"});
}

void Renderer::setupShader(Shader &shader)
{
    using enum Texture;

    shader.setUniformBlock("matrices", 0);

    shader.setInt("albedoMap", static_cast<GLuint>(ALBEDO));
    shader.setInt("armMap", static_cast<GLuint>(ARM));
    shader.setInt("normalMap", static_cast<GLuint>(NORMAL));
    shader.setInt("cubeMap", static_cast<GLuint>(CUBEMAP));
    shader.setInt("shadowMap", static_cast<GLuint>(SHADOW));

    shader.setFloat("nearPlane", cameraDefaultValues::nearPlane);
    shader.setFloat("farPlane", cameraDefaultValues::farPlane);

    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    shader.setFloat("gamma", graphicsSettings_.gamma);
}

void Renderer::updateShaderUniforms(Scene &scene, bool lightDirectionChanged)
{
    DirectionalLight &globalLight{scene.getGlobalLightSource()};
    const glm::mat4 &lightViewProjectionMatrix{globalLight.getViewProjectionMatrix()};

    for (auto &[_, shader] : shaders_)
    {
        shader.setFloat("time", (float)glfwGetTime());

        if (lightDirectionChanged)
        {
            shader.setMat4("lightViewProjection", lightViewProjectionMatrix);
            shader.setVec3("lightDir", globalLight.getDirection());
        }
    }
}

void Renderer::setSceneShaderUniforms(Scene &scene)
{
    const glm::mat4 &lightViewProjectionMatrix{scene.getGlobalLightSource().getViewProjectionMatrix()};

    for (auto &[_, shader] : shaders_)
    {
        shader.setVec3("ambientLight", scene.getAmbientLight());
        shader.setVec3("lightColor", scene.getGlobalLightSource().getColor());
        shader.setMat4("lightViewProjection", lightViewProjectionMatrix);

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
    updateUBO(scene);
    setSceneShaderUniforms(scene);
    updateShadowMaps(scene);
    resetVisibility(scene);
    cullRenderables(scene);

    threadPool_.stopAllThreads();
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager_;
}

void Renderer::deleteBuffers()
{
    glDeleteBuffers(1, &UBO_);
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