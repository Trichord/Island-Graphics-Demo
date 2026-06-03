#include "renderer/renderer.h"

#include <cstdint>
#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "renderer/cascade_shadow.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/scene.h"

namespace
{
// Reduce the distance of the camera's far plane by 80% for shadow cascades.
constexpr float farPlaneReduction{0.2f};
} // namespace

// Render objects, grouped by shader type
void Renderer::renderScene()
{
    if (drawBuckets_.empty())
        return;

    glState_.setVAO(VAOs_.vao);

    GraphicsShader *shader{&shaders_.front()};
    ShaderType currentShaderType{shader->getType()};
    shader->use();

    fastgltf::AlphaMode currentAlphaMode{drawBuckets_.front().alphaMode};

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum ShaderType;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (currentShaderType != mdb.shaderType)
        {
            if (mdb.shaderType == skybox)
                glState_.setDepthFunction(GL_LEQUAL);

            if (currentShaderType == skybox)
                glState_.setDepthFunction(GL_LESS);

            shader = &shaders_[static_cast<int>(mdb.shaderType)];
            shader->use();
            currentShaderType = mdb.shaderType;
        }

        if (currentAlphaMode != mdb.alphaMode)
        {
            glState_.setAlphaMode(mdb.alphaMode);
            currentAlphaMode = mdb.alphaMode;
        }

        GLenum mode{static_cast<GLenum>(mdb.shaderType == particles ? GL_POINTS : GL_TRIANGLES)};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT,
                                         (void *)(mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)),
                                         static_cast<GLintptr>(i * sizeof(std::uint32_t)), mdb.maxDrawCalls, 0);
    }

    // If skybox is last, the shader / state change never triggers and depth function gets left on GL_LEQUAL
    glState_.setDepthFunction(GL_LESS);
}

void Renderer::renderShadowMaps(Scene &scene, bool lightChanged)
{
    glState_.setVAO(VAOs_.shadowVAO);
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);
    glEnable(GL_POLYGON_OFFSET_FILL);

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

    // Splits never change, only set on renderer initialization
    const bool updateFullUBO{framesRendered_ == 0};
    updateCascadeShadowUBO(cascades, lightChanged, updateFullUBO);

    // Only update outer cascade if the light changed
    int cascadeCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    shadowPassTarget_.bind();

    // Tighter cascades depend on player location and have to be updated
    for (int i{0}; i < cascadeCount; ++i)
    {
        using enum ShaderType;

        const glm::mat4 lightVP{cascades[i].viewProjection};

        shadowPassTarget_.setTextureLayer(i);
        shadowPassTarget_.clear();

        cullScene(lightVP);

        // Opaque geometry first
        shadowShader.use();
        shadowPass(shadowShader, i, false);

        // Alpha tested geometry
        shadowAlphaShader.use();
        shadowPass(shadowAlphaShader, i, true);
    }

    shadowPassTarget_.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::shadowPass(GraphicsShader &shader, int cascade, bool alphaTested)
{
    shader.setInt("cascade", cascade);

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum ShaderType;
        using enum fastgltf::AlphaMode;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (mdb.shaderType == ocean || mdb.shaderType == skybox)
            continue;

        if (mdb.shaderType == particles && cascade == shadowCascadeCount - 1)
           continue;

        const bool a2c{mdb.alphaMode == Mask};

        if (a2c != alphaTested)
            continue;

        GLenum mode{static_cast<GLenum>(mdb.shaderType == particles ? GL_POINTS : GL_TRIANGLES)};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT,
                                         (void *)(mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)),
                                         static_cast<GLintptr>(i * sizeof(std::uint32_t)), mdb.maxDrawCalls, 0);
    }
}