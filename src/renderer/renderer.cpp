#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#include "camera.h"
#include "config_window.h"
#include "directional_light.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "instance_data.h"
#include "point_light.h"
#include "render_target.h"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "texture_units.h"
#include "texture_utilities.h"
#include "transform.h"
#include "world_vectors.h"

// Normalized wind direction, east southeast with a slight downward angle
constexpr float windStrength{0.07f};
constexpr glm::vec3 windDir{0.892079f, -0.101373f, 0.440362f};

// Bright midday sun
constexpr glm::vec3 ambientLight{0.2f};
constexpr glm::vec3 lightColor{1.0f, 0.95f, 0.9f};

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.25f};
constexpr float alphaCutoff{0.5f};

Renderer::Renderer(const ConfigWindow::GraphicsSettings &settings)
    : textureManager{threadPool}, graphicsSettings{settings}
{
    setupTextureManager();
    loadShaders();

    for (auto &[_, shader] : shaders)
        setupShader(shader);
}

Renderer::~Renderer()
{
    deleteBuffers();
}

void Renderer::renderScene(Scene &scene)
{
    glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor.data());
    glClearNamedFramebufferfv(0, GL_DEPTH, 0, &depthClear);

    if (scene.getCamera().hasUpdated())
        updateUBO(scene);

    updateShaderUniforms(scene);

    resetVisibility(scene);
    cullRenderables(scene);

    glNamedBufferSubData(SSBO, 0, ssboData.size() * sizeof(SSBOData), ssboData.data());

    renderAllSceneObjects(scene);
}

void Renderer::updateUBO(Scene &scene)
{
    Camera &camera{scene.getCamera()};
    uboData.viewMatrix = camera.getViewMatrix();
    uboData.projectionMatrix = camera.getProjectionMatrix();
    uboData.viewProjectionMatrix = camera.getViewProjectionMatrix();
    uboData.inverseViewProjectMatrix = glm::inverse(uboData.viewProjectionMatrix);
    uboData.cameraPosition = glm::vec4{camera.getPosition(), 0.0f};

    glNamedBufferSubData(UBO, 0, sizeof(PackedUBOData), &uboData);
}

void Renderer::setupTextureManager()
{
    if (graphicsSettings.anisotropicFiltering)
        textureManager.setAnisoLevel(static_cast<float>(graphicsSettings.anisotropicFiltering));
}

void Renderer::loadShaders()
{
    using enum ShaderType;

    shaders.emplace(FLOWERS, Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
    shaders.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
    shaders.emplace(POINT_PBR, Shader{"./shaders/point_PBR.vert", "./shaders/point_PBR.frag"});
    shaders.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
    shaders.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert", "./shaders/reflective.frag"});
    shaders.emplace(SKYBOX, Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
    shaders.emplace(WATER, Shader{"./shaders/water.vert", "./shaders/water.frag"});
    shaders.emplace(OCEAN, Shader{"./shaders/water.vert", "./shaders/ocean.frag"});
}

void Renderer::setupShader(Shader &shader)
{
    using enum Texture;

    shader.setUniformBlock("matrices", 0);

    shader.setInt("albedoMap", ALBEDO);
    shader.setInt("armMap", ARM);
    shader.setInt("normalMap", NORMAL);
    shader.setInt("cubeMap", CUBEMAP);
    shader.setInt("reflectionMap", REFLECTION);
    shader.setInt("refractionMap", REFRACTION);
    shader.setInt("depthMap", DEPTH);

    shader.setFloat("nearPlane", cameraDefaultValues::nearPlane);
    shader.setFloat("farPlane", cameraDefaultValues::farPlane);

    shader.setVec3("ambientLight", ambientLight);
    shader.setVec3("lightColor", lightColor);

    shader.setFloat("windStrength", windStrength);
    shader.setVec3("windDirection", windDir);

    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    shader.setFloat("gamma", graphicsSettings.gamma);
}

void Renderer::updateShaderUniforms(Scene &scene)
{
    DirectionalLight &globalLight{scene.getGlobalLightSource()};
    bool dirUpdated{globalLight.directionHasUpdated()};

    std::vector<PointLight> &pointLights{scene.getPointLights()};

    bool posUpdated{false};
    glm::vec3 pointPos{0.0f};
    if (!pointLights.empty())
    {
        pointPos = pointLights[0].getPosition();
        posUpdated = pointLights[0].positionHasUpdated();
    }

    // Set uniforms
    for (auto &[_, shader] : shaders)
    {
        using enum ShaderType;
        if (dirUpdated)
            shader.setVec3("lightDir", globalLight.getDirection());
        if (posUpdated)
            shader.setVec3("lightPosition", pointPos);

        shader.setFloat("time", (float)glfwGetTime());
    }
}

void Renderer::updateSSBO(IRenderable &object)
{
    auto instances = object.getInstanceData();
    // Move visible instances to front of vector
    auto invisibleInstances = std::ranges::partition(instances,
                               [](const InstanceData &instance) { return instance.isVisible; });

    size_t instanceCount{static_cast<size_t>(invisibleInstances.begin() - instances.begin())};

    object.setVisibleInstanceCount(instanceCount);

    // Sort front to back for speed on depth testing, over proper alpha blending
    std::sort(instances.begin(), instances.begin() + instanceCount,
              [](const InstanceData &a, const InstanceData &b) { return a.sortKey < b.sortKey; });

    std::size_t offset{ssboOffsets.at(object.getHandle())};

    for (std::size_t i{0}; i < instanceCount; ++i)
    {
        ssboData[offset + i].modelMatrix = instances[i].modelMatrix;
        ssboData[offset + i].normalMatrix = instances[i].normalMatrix;
    }
}

void Renderer::initializeResources(Scene &scene)
{
    createBuffers(scene);
    populateSSBO(scene);
    generateCubeMaps(scene);
    borrowSkyboxes(scene);
    updateUBO(scene);
}

void Renderer::borrowSkyboxes(Scene &scene)
{
    using enum ShaderType;
    auto &map = scene.getRenderablesByShader();

    if (!map.contains(SKYBOX) || !map.contains(OCEAN))
        return;

    auto &skyboxes = map[SKYBOX];
    auto &oceans = map[OCEAN];

    if (skyboxes.empty() || oceans.empty())
        return;

    for (int i{0}; i < skyboxes.size() && i < oceans.size(); ++i)
    {
        auto &ocean = oceans[i].object.get();
        auto &skybox = skyboxes[i].object.get();

        textureManager.borrowAssignMaterials(ocean.getHandle(), skybox.getHandle(), CUBEMAP);
    }
}

void Renderer::createBuffers(Scene &scene)
{
    // UBO
    glCreateBuffers(1, &UBO);

    glNamedBufferStorage(UBO, sizeof(PackedUBOData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);

    // SSBO
    glCreateBuffers(1, &SSBO);
    std::size_t numInstances{0};

    // Find total number of all instances across all scene objects
    for (auto &[_, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
            numInstances += renderable.object.get().getInstanceData().size();

    ssboData.reserve(numInstances);

    glNamedBufferStorage(SSBO, numInstances * sizeof(SSBOData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SSBO);
}

void Renderer::generateCubeMaps(Scene &scene)
{
    using enum ShaderType;

    // For iterating cubemap faces
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    // Helper for rendering the scene
    auto renderFace = [&](RenderTarget &target, WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateUBO(scene);
        target.bind();
        target.clear();
        renderAllSceneObjects(scene);
    };

    auto &waters{scene.getRenderablesByShader().at(WATER)};

    if (waters.empty())
        return;

    Camera &camera{scene.getCamera()};

    // Half size render targets for water reflections
    int size{std::max(graphicsSettings.screenWidth, graphicsSettings.screenHeight) / 2};
    int mipLevels = static_cast<int>(std::floor(std::log2(size)) + 1);

    // Save camera settings before we make it bounce around the scene
    const glm::vec3 originalPos{camera.getPosition()};
    float originalPitch{camera.getPitch()};
    float originalYaw{camera.getYaw()};
    camera.setFOV(90.0f, 1.0f);

    RenderTarget cubeFace(size, size, graphicsSettings.msaa, false);
    RenderTarget resolveTarget(size, size, 0, false);

    for (auto &waterPlane : waters)
    {
        camera.setPosition(waterPlane.properties.transform.getPosition());

        waterPlane.properties.isVisible = false;

        // Create the cubemap texture
        GLuint cubeMapTextureID{0};
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubeMapTextureID);
        glTextureStorage2D(cubeMapTextureID, mipLevels, GL_SRGB8_ALPHA8, size, size);

        // Copy each MSAA direction into a cubemap face
        for (GLuint i{0}; i < 6; ++i)
        {
            cubeFace.clear();
            resolveTarget.clear();
            renderFace(cubeFace, faces[i], camera);
            cubeFace.resolveTo(resolveTarget);
            textureUtilities::generateCubemapFace(cubeMapTextureID, resolveTarget.getColorTextureID(), size, i);
        }

        // Texture params
        glGenerateTextureMipmap(cubeMapTextureID);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Assign the texture
        auto &obj = waterPlane.object.get();
        textureManager.setMaterial(obj.getHandle(), CUBEMAP, cubeMapTextureID);
        waterPlane.properties.isVisible = true;
    }

    // Reset
    camera.resetFOV();
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateUBO(scene);

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, graphicsSettings.screenWidth, graphicsSettings.screenHeight);
}

void Renderer::populateSSBO(Scene &scene)
{
    GLuint offset{0};

    for (auto &[_, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
        {
            auto &object = renderable.object.get();

            if (!object.getInstanceCount())
                continue;

            // Keep track of where each model's start position is in the SSBO. Even
            // with sorting and changing instance count, the starting offset into the
            // SSBO will never change.
            ssboOffsets.emplace(object.getHandle(), offset);

            // Build and track each instance's offset into the buffer
            for (auto &instance : object.getInstanceData())
                ssboData.emplace_back(instance.modelMatrix, instance.normalMatrix);

            offset += static_cast<GLuint>(object.getInstanceData().size());
        }

    glNamedBufferSubData(SSBO, 0, ssboData.size() * sizeof(SSBOData), ssboData.data());
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager;
}

void Renderer::deleteBuffers()
{
    glDeleteBuffers(1, &UBO);
    glDeleteBuffers(1, &SSBO);
}

void Renderer::allResourcesLoaded()
{
    threadPool.stopAllThreads();
}