#include "cull_method.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "primitive.h"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_units.h"

// Reduce the distance of the camera's far plane by 80% for shadow cascades. (1024 res)
constexpr float farPlaneReduction{0.2f};

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects(Scene &scene)
{
    using enum ShaderType;

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
    {
        if (objectList.empty() || shaderType == COUNT)
            continue;

        Shader &shader{shaders_.at(shaderType)};
        shader.use();

        if (shaderType == SKYBOX)
            glDepthFunc(GL_LEQUAL);

        for (auto &renderable : objectList)
        {
            if (!renderable.properties.isVisible)
                continue;

            const auto &object = renderable.objectRef.get();
            const auto &properties = renderable.properties;

            shader.setIntU("matricesOffset", properties.matricesBufferBaseIndex);
            shader.setIntU("indicesOffset", properties.indicesBufferIndex);
            shader.setVec3("scale", properties.instances[0].transform.getScale());

            // Draw it
            renderObject(renderable);
        }

        if (shaderType == SKYBOX)
            glDepthFunc(GL_LESS);
    }
}

void Renderer::renderObject(Renderable &renderable)
{
    auto &object = renderable.objectRef.get();
    const auto &properties = renderable.properties;
    const auto &materials{textureManager_.getMaterials(object.getHandle())};
    const GLsizei instanceCount{static_cast<GLsizei>(properties.visibleInstances)};

    for (const Primitive::Mesh &mesh : object.getMeshes())
    {
        if (!mesh.isVisible)
            continue;

        for (const Primitive &primitive : mesh.primitives)
        {
            glState_.setAlphaMode(primitive.getAlphaMode());

            GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};
            GLuint indexOffset{primitive.getEBOIndex()};
            GLuint baseVertex{primitive.getVAOIndex()};

            if (primitive.materialsExist())
            {
                std::size_t materialIndex{primitive.getMaterialIndex()};
                bindTextures(materials[materialIndex]);
            }

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                              (void *)(indexOffset * sizeof(GLuint)), instanceCount, baseVertex);
        }
    }
}

void Renderer::bindTextures(const Material &materials)
{
    using enum Texture;

    if (materials.albedo)
        glState_.bindTexture(static_cast<GLuint>(ALBEDO), materials.albedo);

    if (materials.occlusionRoughnessMetallic)
        glState_.bindTexture(static_cast<GLuint>(ARM), materials.occlusionRoughnessMetallic);

    if (materials.normal)
        glState_.bindTexture(static_cast<GLuint>(NORMAL), materials.normal);

    if (materials.cubemap)
        glState_.bindTexture(static_cast<GLuint>(CUBEMAP), materials.cubemap);
}

void Renderer::updateCascadeShadowUBO(const CascadeShadow::CascadeSplits &cascades,
                                      const CascadeShadow::LightMatrices &matrices)
{
    static_assert(CascadeShadow::cascadeCount == 3);

    packedShadowCascadeData_.splits = {cascades.splits[0].far, cascades.splits[1].far, cascades.splits[2].far, 0.0f};
    memcpy(packedShadowCascadeData_.matrices, matrices.viewProjections, sizeof(packedShadowCascadeData_.matrices));

    glNamedBufferSubData(shadowCascadesUBO_, 0, sizeof(PackedShadowCascadeData), &packedShadowCascadeData_);
}

void Renderer::updateShadowMaps(Scene &scene)
{
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);

    // Get the new orthographic projections and upload to a UBO
    Camera &camera{scene.getCamera()};
    cascadeShadow_.updateCascadeShadows(camera.getResizedViewProjectionMatrix(farPlaneReduction),
                                        scene.getGlobalLightSource(), graphicsSettings_.shadowResolution,
                                        camera.getFarPlane() * farPlaneReduction);

    const auto &splitDistances = cascadeShadow_.getSplitDistances();
    const auto &lightMatrices = cascadeShadow_.getLightMatrices();
    updateCascadeShadowUBO(splitDistances, lightMatrices);

    shadowPassTarget_.bind();
    // Draw the outer cascade first, then cull and work our way closer
    for (int cascade{CascadeShadow::cascadeCount - 1}; cascade >= 0; --cascade)
    {
        shadowPassTarget_.clear();

        // Opaque geometry first
        Shader &shadowShader{shaders_[ShaderType::SHADOW_MAP]};
        shadowShader.use();
        shadowPass(scene, lightMatrices, shadowShader, cascade, false);

        // Use culling and indexed drawing for the inner 2 cascades
        if (cascade == 1)
            cullRenderables(scene, lightMatrices.viewProjections[cascade]);

        // Alpha tested geometry
        Shader &shadowAlphaShader{shaders_[ShaderType::SHADOW_MAP_ALPHA]};
        shadowAlphaShader.use();
        shadowPass(scene, lightMatrices, shadowAlphaShader, cascade, true);

        shadowBlitTarget_.setTextureLayer(static_cast<GLuint>(cascade));
        shadowPassTarget_.blitTo(shadowBlitTarget_);
    }

    shadowPassTarget_.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::shadowPass(Scene &scene, const CascadeShadow::LightMatrices &lightMatrices, Shader &shader, int cascade,
                          bool drawAlphaTested)
{
    const glm::mat4 lightVP{lightMatrices.viewProjections[cascade]};
    shader.setMat4("lightViewProjection", lightVP);
    shader.setBool("useIndices", cascade != 2);

    for (auto &[shaderType, renderables] : scene.getRenderablesByShader())
    {
        using enum ShaderType;

        if (shaderType == SKYBOX || shaderType == OCEAN || shaderType == COUNT)
            continue;

        // Never try and draw grass outside the closest cascade
        if (shaderType == GRASS && cascade != 0)
            continue;

        // Only draw grass or trees if alpha tested geometry has been requested
        bool alphaTestedShader{shaderType == GRASS || shaderType == TREES};

        if (drawAlphaTested != alphaTestedShader)
            continue;

        for (auto &renderable : renderables)
        {
            if (!renderable.properties.isVisible)
                continue;

            auto &object = renderable.objectRef.get();
            const auto &properties = renderable.properties;
            const auto &materials{textureManager_.getMaterials(object.getHandle())};

            // Offsets into the main buffer and the indices buffer
            shader.setIntU("matricesOffset", properties.matricesBufferBaseIndex);

            if (cascade != 2)
                shader.setIntU("indicesOffset", properties.indicesBufferIndex);

            // Instance count
            GLsizei instanceCount{0};
            if (cascade == 2)
                instanceCount = static_cast<GLsizei>(properties.instances.size());
            else
                instanceCount = properties.visibleInstances;

            // Render primitives
            for (const Primitive::Mesh &mesh : object.getMeshes())
            {
                for (const Primitive &primitive : mesh.primitives)
                {
                    GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};
                    GLuint indexOffset{primitive.getEBOIndex()};
                    GLuint baseVertex{primitive.getVAOIndex()};

                    if (primitive.materialsExist() && drawAlphaTested)
                    {
                        std::size_t materialIndex{primitive.getMaterialIndex()};
                        bindTextures(materials[materialIndex]);
                    }

                    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                                      (void *)(indexOffset * sizeof(GLuint)), instanceCount,
                                                      baseVertex);
                }
            }
        }
    }
}
