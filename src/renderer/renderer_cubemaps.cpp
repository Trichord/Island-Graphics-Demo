#include "renderer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "cascade_shadow.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "render_target.h"
#include "renderable.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"
#include "world_vectors.h"

void Renderer::borrowSkybox(const std::vector<DrawEntry> &entries)
{
    using enum ShaderType;

    auto skyboxIter = std::ranges::lower_bound(entries, ShaderType::skybox, {}, &DrawEntry::shaderType);
    auto oceanIter = std::ranges::lower_bound(entries, ShaderType::ocean, {}, &DrawEntry::shaderType);

    if (skyboxIter == entries.end() || oceanIter == entries.end())
        return;

    Handle skyboxHandle{skyboxIter->handle};
    std::unordered_set<Handle> seenHandles;

    while (oceanIter != entries.end() && oceanIter->shaderType == ShaderType::ocean)
    {
        Handle handle{oceanIter->handle};
        ++oceanIter;

        if (seenHandles.contains(handle))
            continue;

        textureManager_.borrowCubemaps(handle, skyboxHandle);
        seenHandles.insert(handle);
    }
}

void Renderer::createCubemaps(Scene &scene, const std::vector<DrawElementsIndirectCommand> &commandsSource,
                              const std::vector<DrawEntry> &entries, SamplerArrayEntry reservedEntry)
{
    using enum ShaderType;

    std::vector<DrawElementsIndirectCommand> commands = commandsSource;

    std::size_t waterIter{0};

    while (entries[waterIter].shaderType != water && waterIter < entries.size())
        ++waterIter;

    if (waterIter == entries.size())
        return;

    // Ensure reflections don't capture uninitialized other reflections or self geometry
    while (entries[waterIter].shaderType == water)
    {
        commands[waterIter].count = 0;
        ++waterIter;
    }

    // Update the draw command buffer now that water is invisible
    GLsizeiptr drawCommandBufferSize{static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand))};
    glNamedBufferSubData(drawCommandsBuffer_, 0, drawCommandBufferSize, commands.data());

    // Save camera settings before we make it bounce around the scene
    Camera &camera{scene.getCamera()};
    const glm::vec3 originalPos{camera.getPosition()};
    const float originalPitch{camera.getPitch()};
    const float originalYaw{camera.getYaw()};
    const float originalFOV{camera.getFOV()};
    const float originalAspect{camera.getAspectRatio()};

    // 90 degrees for cubemap
    camera.setFOV(90.0f);
    camera.setAspectRatio(1.0f);

    int faceSize{getReflectionResolution()};
    glViewport(0, 0, faceSize, faceSize);

    GLenum colorFormat{textureManager_.getReflectionColorFormat()};
    RenderTarget cubeFaces{faceSize, faceSize,
                           RenderTarget::Settings{.msaa = graphicsSettings_.msaa,
                                                  .depthType = RenderTarget::DepthType::TEXTURE,
                                                  .internalFormat = colorFormat,
                                                  .depthFormat = GL_DEPTH_COMPONENT24,
                                                  .textureArray = true,
                                                  .textureArrayDepth = 6}};

    RenderTarget blitTarget{faceSize, faceSize,
                            RenderTarget::Settings{.msaa = 0,
                                                   .depthType = RenderTarget::DepthType::NONE,
                                                   .internalFormat = colorFormat,
                                                   .depthFormat = GL_NONE,
                                                   .textureArray = true,
                                                   .textureArrayDepth = 6}};

    cubeFaces.bind();

    // Water should not be instanced, limit one cubemap per model. By walking the scene instead of draw entries,
    // we ensure each model gets one cubemap
    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType != water)
            continue;

        const Renderable &renderable{entry.renderableRef.get()};

        if (std::optional<Handle> borrowed = renderable.getBorrowedHandle(); borrowed.has_value())
        {
            textureManager_.borrowCubemaps(renderable.getHandle(), borrowed.value());
            continue;
        }

        Transform &transform{entry.transforms.front()};

        // Set the camera in the center of the water plane
        camera.setPosition(transform.getPosition());
        renderCubeFaces(scene, cubeFaces, blitTarget);

        // Assign the texture
        auto &waterPlane = entry.renderableRef.get();
        textureManager_.setCubemap(waterPlane.getHandle(), blitTarget, reservedEntry);

        ++reservedEntry.layer;
    }

    // Reset
    camera.setFOV(originalFOV);
    camera.setAspectRatio(originalAspect);
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateCameraUBO(scene);

    cubeFaces.unbind();
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget)
{
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    auto renderFace = [&](WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateCameraUBO(scene);
        renderScene();
    };

    Camera &camera{scene.getCamera()};

    for (int i{0}; i < 6; ++i)
    {
        target.setTextureLayer(i);
        blitTarget.setTextureLayer(i);

        target.clear();
        renderFace(faces[i], camera);

        target.blitTo(blitTarget);
    }
}