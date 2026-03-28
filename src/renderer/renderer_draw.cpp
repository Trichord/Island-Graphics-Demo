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

            auto &object = renderable.objectRef.get();
            auto &properties = renderable.properties;
            auto iter = modelBaseIndices_.find(object.getHandle());

            if (iter == modelBaseIndices_.end())
                throw std::runtime_error("ERROR: Missing handle in SSBO lookup map");

            ssboOffsets offsets{iter->second};

            shader.setIntU("matricesOffset", offsets.matricesOffset);
            shader.setIntU("indicesOffset", offsets.indicesOffset);
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
    auto &properties = renderable.properties;
    const auto &materials{textureManager_.getMaterials(object.getHandle())};

    for (const Primitive::Mesh &mesh : object.getMeshes())
    {
        if (!mesh.isVisible)
            continue;

        for (const Primitive &primitive : mesh.primitives)
        {
            glState_.setAlphaMode(primitive.getAlphaMode());

            if (primitive.materialsExist())
            {
                std::size_t materialIndex{primitive.getMaterialIndex()};
                bindTextures(materials[materialIndex]);
            }

            glBindVertexArray(primitive.getVAO());

            GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};
            GLsizei instanceCount{static_cast<GLsizei>(properties.visibleInstances)};

            glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0, instanceCount);
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

    Shader &shadowPassShader{shaders_[ShaderType::SHADOW_MAP]};
    shadowPassShader.use();
    shadowPassTarget_.bind();

    // Get the new orthographic projections and upload to a UBO
    Camera &camera{scene.getCamera()};
    cascadeShadow_.updateCascadeShadows(camera.getResizedViewProjectionMatrix(farPlaneReduction),
                                        scene.getGlobalLightSource(), camera.getFarPlane() * farPlaneReduction);

    auto &splitDistances = cascadeShadow_.getSplitDistances();
    auto &lightMatrices = cascadeShadow_.getLightMatrices();
    updateCascadeShadowUBO(splitDistances, lightMatrices);

    // Shadow pass
    for (int cascade{CascadeShadow::cascadeCount - 1}; cascade >= 0; --cascade)
    {
        shadowPassTarget_.clear();

        const glm::mat4 lightVP{lightMatrices.viewProjections[cascade]};

        // Only show grass on the closest cascade
        if (cascade == 0)
            shadowPassShader.setBool("useIndices", true);

        else if (cascade == 1)
        {
            cullRenderables(scene, lightVP);
            shadowPassShader.setBool("useIndices", false);
        }
        else
            shadowPassShader.setBool("useIndices", false);

        shadowPassShader.setMat4("lightViewProjection", lightVP);

        for (auto &[shaderType, renderables] : scene.getRenderablesByShader())
        {
            using enum ShaderType;

            if (shaderType == SKYBOX || shaderType == OCEAN || shaderType == COUNT)
                continue;

            if (shaderType == GRASS && cascade != 0)
                continue;

            for (auto &renderable : renderables)
            {
                if (!renderable.properties.isVisible)
                    continue;

                auto &object = renderable.objectRef.get();
                auto &properties = renderable.properties;
                auto iter = modelBaseIndices_.find(object.getHandle());

                if (iter == modelBaseIndices_.end())
                    throw std::runtime_error("ERROR: Missing handle in SSBO lookup map");

                ssboOffsets offsets{iter->second};

                shadowPassShader.setIntU("matricesOffset", offsets.matricesOffset);

                if (cascade == 0)
                    shadowPassShader.setIntU("indicesOffset", offsets.indicesOffset);

                for (const Primitive::Mesh &mesh : object.getMeshes())
                {
                    for (const Primitive &primitive : mesh.primitives)
                    {
                        glBindVertexArray(primitive.getVAO());

                        GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};

                        GLsizei instanceCount{0};
                        if (cascade == 0)
                            instanceCount = properties.visibleInstances;
                        else
                            instanceCount = static_cast<GLsizei>(properties.instances.size());

                        glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0, instanceCount);
                    }
                }
            }
        }

        shadowBlitTarget_.setTextureLayer(static_cast<GLuint>(cascade));
        shadowPassTarget_.blitTo(shadowBlitTarget_);
    }

    shadowPassTarget_.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}
