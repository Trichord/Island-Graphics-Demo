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

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

// Render objects, grouped by shader type
void Renderer::renderAllSceneObjects()
{
    if (drawBuckets_.empty())
        return;

    Shader *shader{&shaders_.front()};
    ShaderType currentShaderType{shader->getType()};
    shader->use();

    fastgltf::AlphaMode currentAlphaMode{drawBuckets_.front().alphaMode};

    for (MultiDrawInfo &mdi : drawBuckets_)
    {
        if (currentShaderType != mdi.shaderType)
        {
            using enum ShaderType;

            if (mdi.shaderType == skybox)
                glState_.setDepthFunction(GL_LEQUAL);

            if (currentShaderType == skybox)
                glState_.setDepthFunction(GL_LESS);

            shader = &shaders_[static_cast<int>(mdi.shaderType)];
            shader->use();
            currentShaderType = mdi.shaderType;
        }

        if (currentAlphaMode != mdi.alphaMode)
        {
            glState_.setAlphaMode(mdi.alphaMode);
            currentAlphaMode = mdi.alphaMode;
        }

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    (void *)(mdi.startIndex * sizeof(DrawElementsIndirectCommand)), mdi.count, 0);
    }

    // Assume skybox was last and reset for next frame
    glState_.setDepthFunction(GL_LESS);
}

void Renderer::updateCascadeShadowUBO(const Cascades &cascades)
{
    static_assert(shadowCascadeCount == 3);

    packedCascadeShadows_.splits = {cascades[0].far, cascades[1].far, cascades[2].far, 0.0f};

    for (std::size_t i{0}; i < cascades.size(); ++i)
        packedCascadeShadows_.matrices[i] = cascades[i].viewProjection;

    glNamedBufferSubData(shadowCascadesUBO_, 0, sizeof(PackedCascadeShadows), &packedCascadeShadows_);
}

void Renderer::updateShadowMaps(Scene &scene)
{
    msaaShadowTarget_.bind();
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);

    Shader &shadowShader{shaders_[shaderIndex(ShaderType::shadowMap)]};
    Shader &shadowAlphaShader{shaders_[shaderIndex(ShaderType::shadowMapAlpha)]};

    Camera &camera{scene.getCamera()};
    DirectionalLight &light{scene.getGlobalLightSource()};

    // Move the far plane closer for tighter cascades
    const glm::mat4 cameraVP{camera.getResizedViewProjectionMatrix(farPlaneReduction)};
    float farPlane{camera.getFarPlane() * farPlaneReduction};

    // Recalculate frustum splits and light VP matrices
    const Cascades &cascades{
        cascadeShadow_.updateCascadeShadows(cameraVP, light, graphicsSettings_.shadowResolution, farPlane)};

    updateCascadeShadowUBO(cascades);

    // Outermost cascade is pre-baked
    for (int i{0}; i < shadowCascadeCount; ++i)
    {
        using enum ShaderType;

        const glm::mat4 lightVP{cascades[i].viewProjection};

        msaaShadowTarget_.clear();
        cullScene(scene, lightVP);
        updateDrawCommandBuffer();

        // Opaque geometry first
        shadowShader.use();
        shadowPass(lightVP, shadowShader, false);

        // Alpha tested geometry
        shadowAlphaShader.use();
        shadowPass(lightVP, shadowAlphaShader, true);

        // Hardware filter the variance shadow map
        shadowPassTarget_.setTextureLayer(i);
        msaaShadowTarget_.blitTo(shadowPassTarget_);
    }

    msaaShadowTarget_.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::shadowPass(const glm::mat4 lightVP, Shader &shader, bool alphaTested)
{
    shader.setMat4("lightViewProjection", lightVP);

    for (MultiDrawInfo &mdi : drawBuckets_)
    {
        using enum ShaderType;
        using enum fastgltf::AlphaMode;

        if (mdi.shaderType == skybox || mdi.shaderType == ocean)
            continue;

        const bool a2c{mdi.alphaMode == Mask};

        if (a2c != alphaTested)
            continue;

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    (void *)(mdi.startIndex * sizeof(DrawElementsIndirectCommand)), mdi.count, 0);
    }
}
