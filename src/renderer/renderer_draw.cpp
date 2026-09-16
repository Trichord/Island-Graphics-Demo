#include "renderer/renderer.h"

#include <cstdarg>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/cascade_shadow.h"
#include "renderer/render_order.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/scene.h"

namespace
{
const void *glVoidCast(std::size_t bytes) noexcept
{
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(bytes));
}
} // namespace

void Renderer::renderScene()
{
    if (drawBuckets_.empty())
        return;

    glState_.setVAO(VAOs_.vao);

    constexpr std::size_t bufferIndex{bufferRangeIndex(BufferRange::mainPass)};
    const std::size_t drawBufferSection{bufferIndex * drawCommandsSize_};
    const std::size_t parameterBufferSection{bufferIndex * drawCountsSize_};

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        MultiDrawBucket &mdb{drawBuckets_[i]};

        glState_.setDepthFunction(mdb.state.depthFunction);
        glState_.setDepthWrite(mdb.state.writeDepth);
        glState_.setAlphaMode(mdb.state.alphaMode);
        glState_.bindProgram(graphicsShaders_[shaderIndex(mdb.state.renderOrder)]);

        const std::size_t drawCommandOffset{mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)};
        const std::size_t drawCommandIndex{drawBufferSection + drawCommandOffset};

        const std::size_t parameterBufferOffset{i * sizeof(std::uint32_t)};
        const std::size_t parameterBufferIndex{parameterBufferSection + parameterBufferOffset};
        glMultiDrawElementsIndirectCount(mdb.state.primitiveMode, GL_UNSIGNED_INT, glVoidCast(drawCommandIndex),
                                         static_cast<GLintptr>(parameterBufferIndex), mdb.maxDrawCalls, 0);
    }
}

void Renderer::renderShadowMaps(const Scene &scene, const Camera &camera, bool lightChanged)
{
    // Shadow pass setup
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);

    glState_.bindFramebuffer(shadowPassTarget_.getID());
    glState_.setDepthWrite(true);

    updateCascades(scene, camera);
    const CascadeShadow::Cascades &cascades{cascadeShadow_.getCascades()};

    // Only render the outer cascade if the light changed
    int cascadeCount{lightChanged ? CascadeShadow::cascadeCount : CascadeShadow::cascadeCount - 1};

    // Cull and fill buffers so that both passes have the correct draw commands
    for (int i{0}; i < cascadeCount; ++i)
        cullScene(cullFrustumShader_, cascades.viewProjections[i], BufferRange::cascade0 + i);

    // Opaque geometry first (positions only)
    glState_.bindProgram(shadowShader_);
    glState_.setVAO(VAOs_.shadowVAO);
    glState_.setAlphaMode(AlphaMode::opaque);

    for (int i{0}; i < cascadeCount; ++i)
    {
        shadowPassTarget_.setTextureLayer(i);
        shadowPassTarget_.clear();

        // Manually re-bind ranges since everything is pre-culled
        const BufferRange range{BufferRange::cascade0 + i};
        bindBufferRanges(range);
        shadowPass(shadowShader_, range, i, false);
    }

    // Alpha tested geometry (needs vertex paint, uvs, etc)
    glState_.bindProgram(shadowAlphaShader_);
    glState_.setVAO(VAOs_.vao);
    glState_.setAlphaMode(AlphaMode::mask);

    for (int i{0}; i < cascadeCount; ++i)
    {
        const BufferRange range{BufferRange::cascade0 + i};
        bindBufferRanges(range);
        shadowPassTarget_.setTextureLayer(i);
        shadowPass(shadowAlphaShader_, range, i, true);
    }

    glViewport(0, 0, graphicsSettings_.renderWidth, graphicsSettings_.renderHeight);
}

void Renderer::shadowPass(GraphicsShader &shader, BufferRange range, int cascade, bool alphaTested)
{
    const std::size_t bufferIndex{bufferRangeIndex(range)};
    const std::size_t drawBufferSection{bufferIndex * drawCommandsSize_};
    const std::size_t parameterBufferSection{bufferIndex * drawCountsSize_};

    shader.setInt("cascade", cascade);

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum RenderOrder;
        using enum AlphaMode;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (!mdb.state.castsShadows)
            continue;

        if (mdb.state.renderOrder == particles && cascade == CascadeShadow::cascadeCount - 1)
            continue;

        const bool a2c{mdb.state.alphaMode == mask};

        if (a2c != alphaTested)
            continue;

        const std::size_t drawCommandOffset{mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)};
        const std::size_t drawCommandIndex{drawBufferSection + drawCommandOffset};

        const std::size_t parameterBufferOffset{i * sizeof(std::uint32_t)};
        const std::size_t parameterBufferIndex{parameterBufferSection + parameterBufferOffset};

        glMultiDrawElementsIndirectCount(mdb.state.primitiveMode, GL_UNSIGNED_INT, glVoidCast(drawCommandIndex),
                                         static_cast<GLintptr>(parameterBufferIndex), mdb.maxDrawCalls, 0);
    }
}