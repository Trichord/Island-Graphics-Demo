#include <cstring>

#include "camera.h"
#include "cascade_shadow.h"
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
#include "textures.h"

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects()
{
    using enum ShaderType;

    for (auto &[shaderType, drawEntries] : drawEntriesByShader_)
    {
        if (drawEntries.empty() || shaderType == COUNT)
            continue;

        Shader &shader{shaders_.at(shaderType)};
        shader.use();

        if (shaderType == SKYBOX)
            glDepthFunc(GL_LEQUAL);

        for (DrawEntry &drawEntry : drawEntries)
        {
            if (!drawEntry.isVisible)
                continue;

            BoundedTransform &firstInstance{drawEntry.transformsRef.get()[0]};

            shader.setIntU("matricesOffset", drawEntry.matricesBufferBaseIndex);
            shader.setIntU("indicesOffset", drawEntry.indicesBufferIndex);
            shader.setVec3("scale", firstInstance.transform.getScale());

            // Draw it
            renderObject(shader, drawEntry);
        }

        if (shaderType == SKYBOX)
            glDepthFunc(GL_LESS);
    }
}

void Renderer::renderObject(Shader &shader, DrawEntry &drawEntry)
{
    Renderable &renderable = drawEntry.renderableRef.get();
    const auto &materials{textureManager_.getMaterials(renderable.getHandle())};
    const GLsizei instanceCount{static_cast<GLsizei>(drawEntry.visibleInstances)};

    for (const Primitive::Mesh &mesh : renderable.getMeshes())
    {
        if (!mesh.isVisible)
            continue;

        for (const Primitive &primitive : mesh.primitives)
        {
            glState_.setAlphaMode(primitive.getAlphaMode());

            GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};
            GLuint indexOffset{primitive.getEBOIndex()};
            GLuint baseVertex{primitive.getVAOIndex()};

            if (!materials.empty())
            {
                std::size_t materialIndex{primitive.getMaterialIndex()};
                bindTextures(shader, materials[materialIndex]);
            }

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                              (void *)(indexOffset * sizeof(GLuint)), instanceCount, baseVertex);
        }
    }
}

void Renderer::bindTextures(Shader &shader, const Material &material)
{
    if (material.albedo.array != -1)
    {
        shader.setInt("albedoArray", material.albedo.array);
        shader.setInt("albedoLayer", material.albedo.layer);
    }

    if (material.ARM.array != -1)
    {
        shader.setInt("armArray", material.ARM.array);
        shader.setInt("armLayer", material.ARM.layer);
    }

    if (material.normal.array != -1)
    {
        shader.setInt("normalArray", material.normal.array);
        shader.setInt("normalLayer", material.normal.layer);
    }

    if (material.cubemap)
        glState_.bindTexture(static_cast<GLuint>(TextureUnit::cubemap), material.cubemap);
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
        shadowPass(lightMatrices, shadowShader, cascade, false);

        // Use culling and indexed drawing for the inner 2 cascades
        if (cascade == 1)
            cullRenderables(scene, lightMatrices.viewProjections[cascade]);

        // Alpha tested geometry
        Shader &shadowAlphaShader{shaders_[ShaderType::SHADOW_MAP_ALPHA]};
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

    for (auto &[shaderType, drawEntries] : drawEntriesByShader_)
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

        for (DrawEntry &drawEntry : drawEntries)
        {
            if (!drawEntry.isVisible)
                continue;

            Renderable &renderable{drawEntry.renderableRef.get()};
            const auto &materials{textureManager_.getMaterials(renderable.getHandle())};

            // Offsets into the main buffer and the indices buffer
            shader.setIntU("matricesOffset", drawEntry.matricesBufferBaseIndex);

            if (cascade != 2)
                shader.setIntU("indicesOffset", drawEntry.indicesBufferIndex);

            // Instance count
            GLsizei instanceCount{0};
            if (cascade == 2)
                instanceCount = static_cast<GLsizei>(drawEntry.transformsRef.get().size());
            else
                instanceCount = drawEntry.visibleInstances;

            // Render primitives
            for (const Primitive::Mesh &mesh : renderable.getMeshes())
            {
                for (const Primitive &primitive : mesh.primitives)
                {
                    GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};
                    GLuint indexOffset{primitive.getEBOIndex()};
                    GLuint baseVertex{primitive.getVAOIndex()};

                    if (!materials.empty())
                    {
                        std::size_t materialIndex{primitive.getMaterialIndex()};
                        bindTextures(shader, materials[materialIndex]);
                    }

                    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT,
                                                      (void *)(indexOffset * sizeof(GLuint)), instanceCount,
                                                      baseVertex);
                }
            }
        }
    }
}
