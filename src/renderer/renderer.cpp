#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
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
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_units.h"
#include "texture_utilities.h"
#include "transform.h"
#include "world_vectors.h"

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.25f};
constexpr float alphaCutoff{0.5f};

Renderer::Renderer(const ConfigWindow::GraphicsSettings &settings)
    : textureManager_{threadPool_}, graphicsSettings_{settings}
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

    updateShaderUniforms(scene);

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
    shader.setInt("reflectionMap", REFLECTION);
    shader.setInt("refractionMap", REFRACTION);
    shader.setInt("depthMap", DEPTH);

    shader.setFloat("nearPlane", cameraDefaultValues::nearPlane);
    shader.setFloat("farPlane", cameraDefaultValues::farPlane);

    shader.setFloat("mipScale", mipScale);
    shader.setFloat("alphaCutoff", alphaCutoff);

    shader.setFloat("gamma", graphicsSettings_.gamma);
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
    for (auto &[_, shader] : shaders_)
    {
        using enum ShaderType;
        if (dirUpdated)
            shader.setVec3("lightDir", globalLight.getDirection());
        if (posUpdated)
            shader.setVec3("lightPosition", pointPos);

        shader.setFloat("time", (float)glfwGetTime());
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
    scene.getCamera().setFOV(cameraDefaultValues::fov, static_cast<float>(graphicsSettings_.screenWidth) /
                                                           static_cast<float>(graphicsSettings_.screenHeight));
    loadInstancedData(scene);
    createBuffers(scene);
    generateCubeMaps(scene);
    borrowSkyboxes(scene);
    updateUBO(scene);
    setSceneShaderUniforms(scene);
    cullRenderables(scene);
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
        auto &ocean = oceans[i].objectRef.get();
        auto &skybox = skyboxes[i].objectRef.get();

        textureManager_.borrowAssignMaterials(ocean.getHandle(), skybox.getHandle(), CUBEMAP);
    }
}

void Renderer::createBuffers(Scene &scene)
{
    // UBO
    glCreateBuffers(1, &UBO_);
    glNamedBufferStorage(UBO_, sizeof(PackedCameraData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO_);

    // Find total number of all instances across all scene objects
    std::size_t numInstances{0};
    for (auto &[_, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
            numInstances += renderable.properties.visibleInstances;

    // Instanced model matrices
    glCreateBuffers(1, &matricesSSBO_);
    glNamedBufferStorage(matricesSSBO_, numInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, matricesSSBO_);

    // Indices into the model matrices buffer
    glCreateBuffers(1, &indicesSSBO_);
    glNamedBufferStorage(indicesSSBO_, numInstances * sizeof(std::uint32_t), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indicesSSBO_);

    // Fill the SSBOs with all instanced model data
    populateSSBOs(scene, numInstances);
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
    int size{std::max(graphicsSettings_.screenWidth, graphicsSettings_.screenHeight) / 2};
    int mipLevels = static_cast<int>(std::floor(std::log2(size)) + 1);

    // Save camera settings before we make it bounce around the scene
    const glm::vec3 originalPos{camera.getPosition()};
    float originalPitch{camera.getPitch()};
    float originalYaw{camera.getYaw()};
    camera.setFOV(90.0f, 1.0f);

    RenderTarget cubeFace(size, size, graphicsSettings_.msaa, false);
    RenderTarget resolveTarget(size, size, 0, false);

    for (auto &renderable : waters)
    {
        if (!std::holds_alternative<Transform>(renderable.properties.transform))
            continue;

        Transform &transform{std::get<Transform>(renderable.properties.transform)};

        camera.setPosition(transform.getPosition());

        renderable.properties.isVisible = false;

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
        auto &waterPlane = renderable.objectRef.get();
        textureManager_.setMaterial(waterPlane.getHandle(), CUBEMAP, cubeMapTextureID);
        renderable.properties.isVisible = true;
    }

    // Reset
    camera.resetFOV();
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateUBO(scene);

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::populateSSBOs(Scene &scene, std::size_t totalInstanceCount)
{
    // Reserve max amount for matrices
    std::vector<glm::mat4> modelMatrices;
    modelMatrices.reserve(totalInstanceCount);
    instanceIndices_.reserve(totalInstanceCount);

    GLuint offset{0};
    indicesOffset_ = 0;
    instanceIndices_.clear();

    for (auto &[_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            if (!renderable.properties.isInstanced())
                continue;

            auto &object{renderable.objectRef.get()};
            auto &instances{std::get<std::vector<InstancedData>>(renderable.properties.transform)};

            if (instances.empty())
                continue;

            // Keep track of start position for each model's matrices in the SSBO.
            instancedBaseOffsets_.emplace(object.getHandle(), offset);

            for (uint32_t i{0}; i < instances.size(); ++i)
            {
                // Add the model matrix to one SSBO
                modelMatrices.emplace_back(instances[i].modelMatrix);

                // And the indices to the other
                ++renderable.properties.visibleInstances;
                instanceIndices_.push_back(i);
            }

            offset += static_cast<GLuint>(instances.size());
            indicesOffset_ += static_cast<std::uint32_t>(instances.size());
        }

    glNamedBufferSubData(matricesSSBO_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(std::uint32_t), instanceIndices_.data());
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

void Renderer::loadInstancedData(Scene &scene)
{
    for (auto &[_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            // Make everything visible for cubemap generation
            renderable.properties.isVisible = true;
            renderable.objectRef.get().setVisibility(true);

            auto &object = renderable.objectRef.get();

            if (auto instancedData = object.generateInstancedData(); instancedData.has_value())
            {
                std::vector<InstancedData> data{instancedData.value()};

                renderable.properties.visibleInstances = static_cast<uint32_t>(data.size());
                renderable.properties.transform = std::move(data);
            }
        }
}