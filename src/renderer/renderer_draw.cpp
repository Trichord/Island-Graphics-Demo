#include <cstring>
#include <vector>

#include "camera.h"
#include "cascade_shadow.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects()
{
    using enum ShaderType;

    Shader *shader{&shaders_.front()};
    ShaderType currentShaderType{shader->getType()};
    shader->use();

    for (DrawEntry &entry : drawEntries_)
    {
        if (!entry.anyVisible)
            continue;

        if (currentShaderType != entry.shaderType)
        {
            if (entry.shaderType == ShaderType::skybox)
                glState_.setDepthFunction(GL_LEQUAL);

            if (currentShaderType == ShaderType::skybox)
                glState_.setDepthFunction(GL_LESS);

            shader = &shaders_[static_cast<int>(entry.shaderType)];
            shader->use();
            currentShaderType = entry.shaderType;
        }

        // Draw it
        renderObject(*shader, entry);
    }

    // Assume skybox was last and reset for next frame
    glState_.setDepthFunction(GL_LESS);
}

void Renderer::renderObject(Shader &shader, DrawEntry &drawEntry)
{
    Primitive &primitive{*drawEntry.primitive};

    const std::vector<Material> &materials{textureManager_.getMaterials(drawEntry.handle)};
    const Material &material{materials[primitive.materialIndex]};

    const GLsizei instanceCount{static_cast<GLsizei>(drawEntry.visibleInstances)};
    const GLuint numIndices{static_cast<GLuint>(primitive.indices.size())};
    const GLuint indexOffset{static_cast<GLuint>(drawEntry.eboBaseIndex)};
    const GLuint baseVertex{static_cast<GLuint>(drawEntry.vaoBaseIndex)};

    glState_.setAlphaMode(drawEntry.alphaMode);

    if (material.cubemap)
        glState_.bindTexture(static_cast<GLuint>(TextureUnit::cubemap), material.cubemap);

    glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                                  (void *)(indexOffset * sizeof(GLuint)), instanceCount, baseVertex,
                                                  drawEntry.metadataBufferIndex);
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
        using enum ShaderType;

        shadowPassTarget_.clear();

        // Use culling and indexed drawing for the inner 2 cascades
        if (cascade == 1)
            cullAllModels(scene, lightMatrices.viewProjections[cascade]);

        // Opaque geometry first
        Shader &shadowShader{shaders_[shaderIndex(shadowMap)]};
        shadowShader.use();
        shadowPass(lightMatrices, shadowShader, cascade, false);

        // Alpha tested geometry
        Shader &shadowAlphaShader{shaders_[shaderIndex(shadowMapAlphaTested)]};
        shadowAlphaShader.use();
        shadowPass(lightMatrices, shadowAlphaShader, cascade, true);

        shadowBlitTarget_.setTextureLayer(static_cast<GLuint>(cascade));
        shadowPassTarget_.blitTo(shadowBlitTarget_);
    }

    shadowPassTarget_.unbind();

    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::shadowPass(const CascadeShadow::LightMatrices &lightMatrices, Shader &shader, int cascade,
                          bool drawAlphaTested)
{
    const glm::mat4 lightVP{lightMatrices.viewProjections[cascade]};
    shader.setMat4("lightViewProjection", lightVP);
    shader.setBool("useIndices", cascade != 2);

    for (DrawEntry &entry : drawEntries_)
    {
        using enum ShaderType;

        if (entry.shaderType == skybox || entry.shaderType == ocean || entry.shaderType == count)
            continue;

        // Never try and draw grass outside the closest cascade
        if (entry.shaderType == grass && cascade != 0)
            continue;

        // Only draw grass or trees if alpha tested geometry has been requested
        bool alphaTestedShader{entry.shaderType == grass || entry.shaderType == trees};

        if (drawAlphaTested != alphaTestedShader)
            continue;

        if (!entry.anyVisible)
            continue;

        const std::vector<Material> &materials{textureManager_.getMaterials(entry.handle)};

        // Instance count
        GLsizei instanceCount{0};
        if (cascade == 2)
            instanceCount = static_cast<GLsizei>(entry.transforms->size());
        else
            instanceCount = static_cast<GLsizei>(entry.visibleInstances);

        GLuint numIndices{static_cast<GLuint>(entry.primitive->indices.size())};
        GLuint indexOffset{static_cast<GLuint>(entry.eboBaseIndex)};
        GLuint baseVertex{static_cast<GLuint>(entry.vaoBaseIndex)};

        glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                                      (void *)(indexOffset * sizeof(GLuint)), instanceCount, baseVertex,
                                                      entry.metadataBufferIndex);
    }
}
