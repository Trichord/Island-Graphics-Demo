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
            using enum CullMethod;

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
        glState_.bindTexture(ALBEDO, materials.albedo);

    if (materials.occlusionRoughnessMetallic)
        glState_.bindTexture(ARM, materials.occlusionRoughnessMetallic);

    if (materials.normal)
        glState_.bindTexture(NORMAL, materials.normal);

    if (materials.cubemap)
        glState_.bindTexture(CUBEMAP, materials.cubemap);
}