#include "renderer.h"

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects(Scene &scene)
{
    using enum ShaderType;

    for (auto &[shaderType, objectList] : scene.getRenderablesByShader())
    {
        if (objectList.empty() || shaderType == SKYBOX)
            continue;

        Shader &shader{shaders.at(shaderType)};
        shader.use();

        for (auto &renderable : objectList)
        {
            if (!renderable.properties.isVisible)
                continue;

            glm::mat4 modelMatrix{renderable.properties.transform.getModelMatrix()};
            glm::mat3 normalMatrix{renderable.properties.transform.getNormalMatrix()};

            shader.setMat4("modelMatrix", modelMatrix);
            shader.setMat3("normalMatrix", normalMatrix);
            shader.setVec3("scale", renderable.properties.transform.getScale());

            auto &object = renderable.object.get();
            if (auto iter = ssboOffsets.find(object.getHandle()); iter != ssboOffsets.end())
                shader.setInt("index", iter->second);

            // Draw it
            renderObject(object);
        }
    }
}

void Renderer::renderObject(IRenderable &object)
{
    std::size_t lastMaterialIndex{0};
    bool materialBound{false};
    const auto &materials{textureManager.getMaterials(object.getHandle())};

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

                // Switch materials only if they've changed
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
                GLuint offset{ssboOffsets.at(object.getHandle())};
                GLsizei instanceCount{static_cast<GLsizei>(object.getInstanceCount())};
                glDrawElementsInstanced(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0, instanceCount);
            }

            else
                glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
        }
    }
}

void Renderer::renderSkybox(Scene &scene)
{
    using enum ShaderType;
    auto &map = scene.getRenderablesByShader();

    if (!map.contains(SKYBOX))
        return;

    auto &skyboxes = map[SKYBOX];

    Shader &skyboxShader{shaders.at(SKYBOX)};
    skyboxShader.use();

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    for (auto &renderable : skyboxes)
        renderObject(renderable.object.get());

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void Renderer::bindTextures(const Material &materials)
{
    using enum Texture;

    if (materials.albedo)
        glState.bindTexture(ALBEDO, materials.albedo);

    if (materials.occlusionRoughnessMetallic)
        glState.bindTexture(ARM, materials.occlusionRoughnessMetallic);

    if (materials.normal)
        glState.bindTexture(NORMAL, materials.normal);

    if (materials.cubemap)
        glState.bindTexture(CUBEMAP, materials.cubemap);
}