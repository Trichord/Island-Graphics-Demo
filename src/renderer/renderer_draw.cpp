#include "renderer/renderer.h"

#include <cstdint>
#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

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

    for (std::size_t i{0}; i < drawBuckets_.size(); ++i)
    {
        using enum ShaderType;

        MultiDrawBucket &mdb{drawBuckets_[i]};

        if (mdb.shaderType == skybox)
            glState_.setDepthFunction(GL_LEQUAL);
        else
            glState_.setDepthFunction(GL_LESS);

        glState_.setAlphaMode(mdb.alphaMode);
        glState_.bindProgram(shaders_[shaderIndex(mdb.shaderType)]);

        GLenum mode{static_cast<GLenum>(mdb.shaderType == particles ? GL_POINTS : GL_TRIANGLES)};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT,
                                         (void *)(mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)),
                                         static_cast<GLintptr>(i * sizeof(std::uint32_t)), mdb.maxDrawCalls, 0);
    }
}

void Renderer::renderShadowMaps(Scene &scene, bool lightChanged)
{
    glViewport(0, 0, graphicsSettings_.shadowResolution, graphicsSettings_.shadowResolution);
    glEnable(GL_POLYGON_OFFSET_FILL);
    shadowPassTarget_.bind();

    updateCascades(scene, lightChanged);
    const Cascades &cascades{cascadeShadow_.getCascades()};

    // Only update outer cascade if the light changed
    int cascadeCount{lightChanged ? shadowCascadeCount : shadowCascadeCount - 1};

    for (int i{0}; i < cascadeCount; ++i)
    {
        const glm::mat4 lightVP{cascades[i].viewProjection};

        // Main pass is at buffer[0]
        int bufferSection{1 + i};
        bindBufferRanges(bufferSection);
        cullScene(lightVP, bufferSection);
    }

    // Opaque geometry first (positions only)
    GraphicsShader &shadowShader{shaders_[shaderIndex(ShaderType::shadowMap)]};
    shadowShader.use();
    glState_.setVAO(VAOs_.shadowVAO);

    for (int i{0}; i < cascadeCount; ++i)
    {
        shadowPassTarget_.setTextureLayer(i);
        shadowPassTarget_.clear();

        int bufferSection{1 + i};

        bindBufferRanges(bufferSection);
        shadowPass(shadowShader, i, false);
    }

    // Alpha tested geometry (needs vertex paint, uvs, etc)
    GraphicsShader &shadowAlphaShader{shaders_[shaderIndex(ShaderType::shadowMapAlpha)]};
    shadowAlphaShader.use();
    glState_.setVAO(VAOs_.vao);

    for (int i{0}; i < cascadeCount; ++i)
    {
        bindBufferRanges(1 + i);
        shadowPassTarget_.setTextureLayer(i);
        shadowPass(shadowAlphaShader, i, true);
    }

    shadowPassTarget_.unbind();
    bindBufferRanges(0);
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::shadowPass(GraphicsShader &shader, int cascade, bool alphaTested)
{
    shader.setInt("cascade", cascade);
    int bufferIndex{1 + cascade};

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

        std::size_t drawBufferSection{bufferIndex * drawCommandsSize_};
        std::size_t drawCommandOffset{mdb.drawCommandsBaseIndex * sizeof(DrawElementsIndirectCommand)};
        std::size_t drawCommandIndex{drawBufferSection + drawCommandOffset};

        std::size_t parameterBufferSection{bufferIndex * drawCountsSize_};
        std::size_t parameterBufferOffset{i * sizeof(std::uint32_t)};
        std::size_t parameterBufferIndex{parameterBufferSection + parameterBufferOffset};

        glMultiDrawElementsIndirectCount(mode, GL_UNSIGNED_INT, (void *)(drawCommandIndex),
                                         static_cast<GLintptr>(parameterBufferIndex), mdb.maxDrawCalls, 0);
    }
}

void Renderer::bindBufferRanges(int index)
{
    auto bindSSBO = [](BindingPointSSBO bindingPoint, GLuint buffer, std::uint32_t size, int index) {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(bindingPoint), buffer,
                          static_cast<GLintptr>(size * index), static_cast<GLsizeiptr>(size));
    };

    using enum BindingPointSSBO;

    bindSSBO(drawCommands, SSBOs_.drawCommands, drawCommandsSize_, index);
    bindSSBO(cullResults, SSBOs_.cullResults, cullResultsSize_, index);
    bindSSBO(drawCounts, SSBOs_.drawCounts, drawCountsSize_, index);
    bindSSBO(visibleInstances, SSBOs_.visibleInstances, visibleInstancesSize_, index);
}