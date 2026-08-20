#include "renderer/renderer.h"

#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/cascade_shadow.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/scene.h"

// Render objects, grouped by shader type
void Renderer::renderScene()
{
    if (drawBuckets_.empty())
        return;

    glState_.setVAO(VAOs_.vao);

    constexpr int bufferIndex{bufferRangeIndex(BufferRange::mainPass)};
    const std::size_t drawBufferSection{bufferIndex * drawCommandsSize_};
    const std::size_t parameterBufferSection{bufferIndex * drawCountsSize_};

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum ShaderType;
        using enum AlphaMode;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (mdb.shaderType == skybox)
            glState_.setDepthFunction(GL_LEQUAL);
        else
            glState_.setDepthFunction(GL_LESS);

        // Disable depth writing for blended objects, otherwise Hi-Z culling gets falsely triggered for objects
        // behind the blend
        if (mdb.alphaMode == blend)
            glState_.setDepthWrite(false);
        else
            glState_.setDepthWrite(true);

        glState_.setAlphaMode(mdb.alphaMode);
        glState_.bindProgram(shaders_[shaderIndex(mdb.shaderType)]);

        GLenum mode{static_cast<GLenum>(mdb.shaderType == particles ? GL_POINTS : GL_TRIANGLES)};

        const std::size_t drawCommandOffset{mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)};
        const std::size_t drawCommandIndex{drawBufferSection + drawCommandOffset};

        const std::size_t parameterBufferOffset{i * sizeof(std::uint32_t)};
        const std::size_t parameterBufferIndex{parameterBufferSection + parameterBufferOffset};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT, reinterpret_cast<void *>(drawCommandIndex),
                                         static_cast<GLintptr>(parameterBufferIndex), mdb.maxDrawCalls, 0);
    }
}

void Renderer::renderShadowMaps(const Scene &scene, const Camera &camera, bool lightChanged)
{
    // Shadow pass setup
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);
    glEnable(GL_POLYGON_OFFSET_FILL);

    shadowPassTarget_.bind();
    glState_.setDepthWrite(true);
    glState_.setVAO(VAOs_.shadowVAO);
    glState_.setAlphaMode(AlphaMode::opaque);

    updateCascades(scene, camera, lightChanged);
    const Cascades &cascades{cascadeShadow_.getCascades()};

    // Only update outer cascade if the light changed
    int cascadeCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    // Cull and fill buffers so that both passes have the correct draw commands
    for (int i{0}; i < cascadeCount; ++i)
    {
        const glm::mat4 lightVP{cascades[i].viewProjection};

        cullScene(cullFrustumShader_, lightVP, BufferRange::cascade0 + i);
    }

    // Opaque geometry first (positions only)
    GraphicsShader &shadowShader{shaders_[shaderIndex(ShaderType::shadowMap)]};
    glState_.bindProgram(shadowShader);

    for (int i{0}; i < cascadeCount; ++i)
    {
        shadowPassTarget_.setTextureLayer(i);
        shadowPassTarget_.clear();

        // Manually re-bind ranges since everything is pre-culled
        const BufferRange range{BufferRange::cascade0 + i};
        bindBufferRanges(range);
        shadowPass(shadowShader, range, i, false);
    }

    // Alpha tested geometry (needs vertex paint, uvs, etc)
    GraphicsShader &shadowAlphaShader{shaders_[shaderIndex(ShaderType::shadowMapAlpha)]};
    glState_.bindProgram(shadowAlphaShader);
    glState_.setVAO(VAOs_.vao);

    for (int i{0}; i < cascadeCount; ++i)
    {
        const BufferRange range{BufferRange::cascade0 + i};
        bindBufferRanges(range);
        shadowPassTarget_.setTextureLayer(i);
        shadowPass(shadowAlphaShader, range, i, true);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glViewport(0, 0, graphicsSettings_.renderWidth, graphicsSettings_.renderHeight);
    frameBuffer_.bind();
}

void Renderer::shadowPass(GraphicsShader &shader, BufferRange range, int cascade, bool alphaTested)
{
    const int bufferIndex{bufferRangeIndex(range)};
    const std::size_t drawBufferSection{bufferIndex * drawCommandsSize_};
    const std::size_t parameterBufferSection{bufferIndex * drawCountsSize_};

    shader.setInt("cascade", cascade);

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum ShaderType;
        using enum AlphaMode;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (mdb.shaderType == ocean || mdb.shaderType == skybox)
            continue;

        if (mdb.shaderType == particles && cascade == shadowCascadeCount - 1)
            continue;

        const bool a2c{mdb.alphaMode == mask};

        if (a2c != alphaTested)
            continue;

        GLenum mode{static_cast<GLenum>(mdb.shaderType == particles ? GL_POINTS : GL_TRIANGLES)};

        const std::size_t drawCommandOffset{mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)};
        const std::size_t drawCommandIndex{drawBufferSection + drawCommandOffset};

        const std::size_t parameterBufferOffset{i * sizeof(std::uint32_t)};
        const std::size_t parameterBufferIndex{parameterBufferSection + parameterBufferOffset};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT, reinterpret_cast<void *>(drawCommandIndex),
                                         static_cast<GLintptr>(parameterBufferIndex), mdb.maxDrawCalls, 0);
    }
}