#include "renderer.h"

#include <algorithm>
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
#include "material_slots.h"
#include "point_light.h"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

// Normalized wind direction, east southeast with a slight downward angle
constexpr float windStrength{0.07f};
constexpr glm::vec3 windDir{0.892079f, -0.101373f, 0.440362f};

// Bright midday sun
constexpr glm::vec3 ambientLight{0.15f};
constexpr glm::vec3 lightColor{1.0f, 0.95f, 0.9f};

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.25f};
constexpr float alphaCutoff{0.5f};

Renderer::Renderer(const ConfigWindow::GraphicsSettings &settings)
    : reflectionFBO{settings.screenWidth, settings.screenHeight},
      refractionFBO{settings.screenWidth, settings.screenHeight}, glState{settings.msaa != 0},
      graphicsSettings{settings}
{
    setupTextureManager();
    loadShaders();

    for (auto &[_, shader] : shaders)
        setupShader(shader);
}

Renderer::Renderer(Renderer &&other) noexcept
    : UBO{other.UBO}, uboData{other.uboData}, reflectionFBO{std::move(other.reflectionFBO)},
      refractionFBO{std::move(other.refractionFBO)}, SSBO{other.SSBO}, ssboData{std::move(other.ssboData)},
      ssboOffsets{std::move(other.ssboOffsets)}, glState{std::move(other.glState)},
      textureManager{std::move(other.textureManager)}, shaders{std::move(other.shaders)},
      graphicsSettings{other.graphicsSettings}
{
    other.UBO = 0;
    other.SSBO = 0;
}

Renderer::~Renderer()
{
    deleteBuffers();
}

void Renderer::renderScene(Scene &scene)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    updateShaderUniforms(scene);

    resetVisibility(scene);
    cullRenderables(scene);

    glNamedBufferSubData(SSBO, 0, ssboData.size() * sizeof(SSBOData), ssboData.data());

    renderPrePass(scene);
    renderAllSceneObjects(scene);
    renderSkybox(scene);
}

void Renderer::updateBuffers(Scene &scene)
{
    uboData.viewMatrix = scene.camera.getViewMatrix();
    uboData.viewProjectionMatrix = scene.camera.getViewProjectionMatrix();
    uboData.cameraPosition = glm::vec4{scene.camera.getPosition(), 0.0f};

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

    if (graphicsSettings.msaa)
    {
        shaders.emplace(FLOWERS, Shader{"./shaders/flowers.vert", "./shaders/flowers_msaa.frag"});
        shaders.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees_msaa.frag"});
        shaders.emplace(BLINN_PREPASS, Shader{"./shaders/blinn_prepass.vert", "./shaders/blinn_prepass_msaa.frag"});
        shaders.emplace(BLINN_PREPASS_INSTANCED,
                        Shader{"./shaders/blinn_prepass_instanced.vert", "./shaders/blinn_prepass_msaa.frag"});
    }
    else
    {
        shaders.emplace(FLOWERS, Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
        shaders.emplace(TREES, Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
        shaders.emplace(BLINN_PREPASS, Shader{"./shaders/blinn_prepass.vert", "./shaders/blinn_prepass.frag"});
        shaders.emplace(BLINN_PREPASS_INSTANCED,
                        Shader{"./shaders/blinn_prepass_instanced.vert", "./shaders/blinn_prepass.frag"});
    }

    shaders.emplace(POINT_PBR, Shader{"./shaders/point_PBR.vert", "./shaders/point_PBR.frag"});
    shaders.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert", "./shaders/directional_PBR.frag"});
    shaders.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert", "./shaders/reflective.frag"});
    shaders.emplace(SKYBOX, Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
    shaders.emplace(WATER, Shader{"./shaders/water.vert", "./shaders/water.frag"});
}

void Renderer::setupShader(Shader &shader)
{
    using enum MaterialSlot;

    shader.setUniformBlock("matrices", 0);

    shader.setInt("albedoMap", ALBEDO);
    shader.setInt("armMap", ARM);
    shader.setInt("normalMap", NORMAL);
    shader.setInt("cubeMap", CUBEMAP);
    shader.setInt("reflectionMap", REFLECTION);
    shader.setInt("refractionMap", REFRACTION);
    shader.setInt("depthMap", DEPTH);

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
        shader.setMat4("viewProjectionNoTranslation",
                       scene.camera.getProjectionMatrix() * scene.camera.getViewMatrixNoTranslation());
    }
}

void Renderer::renderPrePass(Scene &scene)
{
    glEnable(GL_CLIP_DISTANCE0);
}

void Renderer::bindTextures(const Material &materials)
{
    using enum MaterialSlot;

    if (materials.albedo)
        glState.bindTexture(ALBEDO, materials.albedo);

    if (materials.occlusionRoughnessMetallic)
        glState.bindTexture(ARM, materials.occlusionRoughnessMetallic);

    if (materials.normal)
        glState.bindTexture(NORMAL, materials.normal);

    if (materials.cubemap)
        glState.bindTexture(CUBEMAP, materials.cubemap);
}

TextureManager &Renderer::getTextureManager()
{
    return textureManager;
}

void Renderer::resetVisibility(Scene &scene)
{
    for (auto &[_, objectList] : scene.getSceneObjects())
        for (auto &renderable : objectList)
        {
            renderable.isVisible = true;
            renderable.object.get().resetVisibility();
        }
}

void Renderer::renderSkybox(Scene &scene)
{
    using enum ShaderType;
    auto &skyboxes = scene.getSceneObjects()[SKYBOX];

    if (skyboxes.empty())
        return;

    Shader &skyboxShader{shaders.at(SKYBOX)};
    skyboxShader.use();

    glDepthFunc(GL_LEQUAL);

    for (auto &renderable : skyboxes)
    {
        auto &skybox = renderable.object.get();

        const std::vector<Material> &materials{textureManager.acquireUniqueTextureSet(skybox.getModelHandle())};

        if (!materials.empty())
            bindTextures(materials[0]);

        for (auto &mesh : skybox.getMeshes())
            for (auto &primitive : mesh.primitives)
            {
                glBindVertexArray(primitive.getVAO());
                glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
            }
    }

    glDepthFunc(GL_LESS);
}

void Renderer::renderAllSceneObjects(Scene &scene)
{
    using enum ShaderType;

    // For each shader type in the scene
    for (auto &[shaderType, objectList] : scene.getSceneObjects())
    {
        if (objectList.empty() || shaderType == SKYBOX)
            continue;

        Shader &shader{shaders.at(shaderType)};
        shader.use();

        // For each object of that shader type
        for (auto &renderable : objectList)
        {
            if (!renderable.isVisible)
                continue;

            glm::mat4 modelMatrix{renderable.transform.getModelMatrix()};
            glm::mat3 normalMatrix{renderable.transform.getNormalMatrix()};

            shader.setMat4("modelMatrix", modelMatrix);
            shader.setMat3("normalMatrix", normalMatrix);

            auto &obj = renderable.object.get();
            if (ssboOffsets.find(obj.getModelHandle()) != ssboOffsets.end())
                shader.setInt("index", ssboOffsets.at(obj.getModelHandle()));

            // Draw it
            auto &object = renderable.object.get();
            renderObject(object);
        }
    }
}

void Renderer::renderObject(Renderable &object)
{
    std::size_t lastMaterialIndex{0};
    bool materialBound{false};
    const auto materials{textureManager.acquireUniqueTextureSet(object.getMaterialID())};

    for (const Primitive::Mesh &mesh : object.getMeshes())
    {
        if (!mesh.isVisible)
            continue;

        for (const Primitive &primitive : mesh.primitives)
        {
            glState.setAlphaMode(primitive.getAlphaMode());

            if (primitive.materialsExist())
            {
                std::size_t materialIndex{primitive.getMaterialIndex()};

                // Switch materials
                if (materialIndex != lastMaterialIndex || !materialBound)
                {
                    lastMaterialIndex = materialIndex;
                    materialBound = true;
                    bindTextures(materials[materialIndex]);
                }
            }

            glBindVertexArray(primitive.getVAO());

            if (object.getInstanceCount())
            {
                GLuint offset{ssboOffsets.at(object.getModelHandle())};
                GLsizei instanceCount{static_cast<GLsizei>(object.getInstanceCount())};
                glDrawElementsInstanced(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0, instanceCount);
            }

            else
                glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
        }
    }
}

void Renderer::updateSSBO(Renderable &object)
{
    auto instances = object.getInstanceData();
    // Move visible instances to front of vector
    auto iter = std::partition(instances.begin(), instances.end(),
                               [](const InstanceData &instance) { return instance.isVisible; });

    size_t instanceCount = static_cast<size_t>(std::distance(instances.begin(), iter));

    object.setVisibleInstanceCount(instanceCount);

    // Sort front to back for speed on depth testing, over proper alpha blending
    std::sort(instances.begin(), instances.begin() + instanceCount,
              [](const InstanceData &a, const InstanceData &b) { return a.sortKey < b.sortKey; });

    std::size_t offset{ssboOffsets.at(object.getModelHandle())};

    for (std::size_t i{0}; i < instanceCount; ++i)
    {
        ssboData[offset + i].modelMatrix = instances[i].modelMatrix;
        ssboData[offset + i].normalMatrix = instances[i].normalMatrix;
    }
}

void Renderer::initializeBuffers(Scene &scene)
{
    // UBO
    glCreateBuffers(1, &UBO);

    glNamedBufferStorage(UBO, sizeof(PackedUBOData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);

    // SSBO
    glCreateBuffers(1, &SSBO);
    std::size_t numInstances{0};

    // Find total number of all instances across all scene objects
    for (auto &[_, objectList] : scene.getSceneObjects())
        for (auto &renderable : objectList)
            numInstances += renderable.object.get().getInstanceData().size();

    ssboData.reserve(numInstances);

    glNamedBufferStorage(SSBO, numInstances * sizeof(SSBOData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SSBO);

    populateSSBO(scene);
}

void Renderer::populateSSBO(Scene &scene)
{
    GLuint offset{0};

    for (auto &[_, objectList] : scene.getSceneObjects())
        for (auto &renderable : objectList)
        {
            auto &object = renderable.object.get();

            if (!object.getInstanceCount())
                continue;

            // Keep track of where each model's start position is in the SSBO. Even
            // with sorting and changing instance count, the starting offset into the
            // SSBO will never change.
            ssboOffsets.emplace(object.getModelHandle(), offset);

            // Build and track each instance's offset into the buffer
            for (auto &instance : object.getInstanceData())
                ssboData.emplace_back(instance.modelMatrix, instance.normalMatrix);

            offset += static_cast<GLuint>(object.getInstanceData().size());
        }
}

void Renderer::deleteBuffers()
{
    glDeleteBuffers(1, &UBO);
    glDeleteBuffers(1, &SSBO);
}