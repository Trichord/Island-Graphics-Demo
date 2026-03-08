#include "cull_method.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderable.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_units.h"

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects(Scene &scene)
{
    using enum ShaderType;

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
    {
        if (objectList.empty())
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

            if (std::holds_alternative<Transform>(properties.transform))
            {
                Transform &transform{std::get<Transform>(properties.transform)};

                shader.setMat4("modelMatrix", transform.getModelMatrix());
                shader.setVec3("scale", transform.getScale());
            }

            if (auto iter = instancedBaseOffsets_.find(object.getHandle()); iter != instancedBaseOffsets_.end())
            {
                shader.setIntU("baseIndex", iter->second);
                shader.setIntU("indicesOffset", renderable.properties.instancedIndicesOffset);
            }

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
            using enum CullMethod;

            glState_.setAlphaMode(primitive.getAlphaMode());

            if (primitive.materialsExist())
            {
                std::size_t materialIndex{primitive.getMaterialIndex()};
                bindTextures(materials[materialIndex]);
            }

            glBindVertexArray(primitive.getVAO());

            GLuint numIndices{static_cast<GLuint>(primitive.getIndices().size())};

            if (properties.cullMethod == INSTANCED)
            {
                GLsizei instanceCount{static_cast<GLsizei>(properties.visibleInstances)};
                glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0, instanceCount);
            }

            else
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
        }
    }
}

void Renderer::bindTextures(const Material &materials)
{
    using enum Texture;

    if (materials.albedo)
        glState_.bindTexture(ALBEDO, materials.albedo);

    if (materials.occlusionRoughnessMetallic)
        glState_.bindTexture(ARM, materials.occlusionRoughnessMetallic);

    if (materials.normal)
        glState_.bindTexture(NORMAL, materials.normal);

    if (materials.cubemap)
        glState_.bindTexture(CUBEMAP, materials.cubemap);
}