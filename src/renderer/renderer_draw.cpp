#include <cstring>
#include <vector>

#include "camera.h"
#include "cascade_shadow.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderer.h"
#include "scene.h"
#include "graphics_shader.h"
#include "texture_manager.h"
#include "textures.h"

// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};

// Render objects, grouped by shader type
void Renderer::renderScene()
{
    if (drawBuckets_.empty())
        return;

    GraphicsShader *shader{&shaders_.front()};
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

void Renderer::renderShadowMaps(Scene &scene, bool lightChanged, bool initAll)
{
    msaaShadowTarget_.bind();
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);

    GraphicsShader &shadowShader{shaders_[shaderIndex(ShaderType::shadowMap)]};
    GraphicsShader &shadowAlphaShader{shaders_[shaderIndex(ShaderType::shadowMapAlpha)]};

    Camera &camera{scene.getCamera()};
    DirectionalLight &light{scene.getGlobalLight()};

    // Move the far plane closer for tighter cascades
    const glm::mat4 cameraVP{camera.getResizedViewProjectionMatrix(farPlaneReduction)};
    float farPlane{camera.getFarPlane() * farPlaneReduction};

    // Recalculate frustum splits and light VP matrices
    const Cascades &cascades{
        cascadeShadow_.updateCascadeShadows(cameraVP, light, graphicsSettings_.shadowResolution, farPlane)};

    updateCascadeShadowUBO(cascades, lightChanged, initAll);

    // Only update outer cascade if the light changed
    int cascadeCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    // Tighter cascades depend on player location and have to be updated
    for (int i{0}; i < cascadeCount; ++i)
    {
        using enum ShaderType;

        const glm::mat4 lightVP{cascades[i].viewProjection};

        msaaShadowTarget_.clear();
        cullScene(scene, lightVP);
        updateDrawCommandBuffer();

        // Opaque geometry first
        shadowShader.use();
        shadowPass(shadowShader, i, false);

        // Alpha tested geometry
        shadowAlphaShader.use();
        shadowPass(shadowAlphaShader, i, true);

        // Hardware filter the variance shadow map
        shadowPassTarget_.setTextureLayer(i);
        msaaShadowTarget_.blitTo(shadowPassTarget_);
    }

    msaaShadowTarget_.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::shadowPass(GraphicsShader &shader, int cascade, bool alphaTested)
{
    shader.setInt("cascade", cascade);

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
