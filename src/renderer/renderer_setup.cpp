#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "camera.h"
#include "config_window.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "render_target.h"
#include "renderable.h"
#include "scene.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_units.h"
#include "texture_utilities.h"
#include "transform.h"
#include "world_vectors.h"

void Renderer::loadInstancedData(Scene &scene)
{
    for (auto &[_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            // Make everything visible for cubemap generation
            renderable.properties.isVisible = true;
            renderable.objectRef.get().setMeshVisibility(true);

            auto &object = renderable.objectRef.get();

            if (auto instancedData = object.generateInstancedData(); instancedData.has_value())
            {
                std::vector<Instance> data{instancedData.value()};

                renderable.properties.visibleInstances = static_cast<uint32_t>(data.size());
                renderable.properties.instances = std::move(data);
            }
        }
}

void Renderer::borrowSkyboxes(Scene &scene)
{
    using enum ShaderType;

    auto &map = scene.getRenderablesByShader();

    if (!map.contains(SKYBOX) || !map.contains(OCEAN))
        return;

    auto &skyboxes = map[SKYBOX];
    auto &oceans = map[OCEAN];

    if (skyboxes.empty() || oceans.empty())
        return;

    for (int i{0}; i < skyboxes.size() && i < oceans.size(); ++i)
    {
        auto &ocean = oceans[i].objectRef.get();
        auto &skybox = skyboxes[i].objectRef.get();

        textureManager_.borrowAssignMaterials(ocean.getHandle(), skybox.getHandle(), CUBEMAP);
    }
}

void Renderer::createBuffers(Scene &scene)
{
    // UBO
    glCreateBuffers(1, &UBO_);
    glNamedBufferStorage(UBO_, sizeof(PackedCameraData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO_);

    // Find total number of all instances across all scene objects
    std::size_t numInstances{0};
    for (auto &[_, objectList] : scene.getRenderablesByShader())
        for (auto &renderable : objectList)
            numInstances += renderable.properties.instances.size();

    // Instanced model matrices
    glCreateBuffers(1, &matricesSSBO_);
    glNamedBufferStorage(matricesSSBO_, numInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, matricesSSBO_);

    // Indices into the model matrices buffer
    glCreateBuffers(1, &indicesSSBO_);
    glNamedBufferStorage(indicesSSBO_, numInstances * sizeof(std::uint32_t), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indicesSSBO_);

    // Fill the SSBOs with all instanced model data
    populateSSBOs(scene, numInstances);
}

void Renderer::createCubemaps(Scene &scene)
{
    using enum ShaderType;

    auto &waters{scene.getRenderablesByShader().at(WATER)};

    if (waters.empty())
        return;

    Camera &camera{scene.getCamera()};

    // Half size render targets for water reflections
    int renderTargetSize{std::max(graphicsSettings_.screenWidth, graphicsSettings_.screenHeight) / 2};
    int mipLevels = static_cast<int>(std::floor(std::log2(renderTargetSize)) + 1);

    RenderTarget cubeFace(renderTargetSize, renderTargetSize, graphicsSettings_.msaa, GL_SRGB8, GL_DEPTH_COMPONENT24,
                          FBODepth::RBO);
    RenderTarget resolveTarget(renderTargetSize, renderTargetSize, 0, GL_SRGB8, GL_NONE, FBODepth::NONE);

    // Save camera settings before we make it bounce around the scene
    const glm::vec3 originalPos{camera.getPosition()};
    float originalPitch{camera.getPitch()};
    float originalYaw{camera.getYaw()};
    camera.setFOV(90.0f, 1.0f);

    for (auto &renderable : waters)
    {
        auto &instances = renderable.properties.instances;

        if (instances.empty())
            continue;

        // Set the camera in the center of the water plane. Water should not be instanced
        Transform &transform{instances[0].transform};
        camera.setPosition(transform.getPosition());

        // Hide self
        renderable.properties.isVisible = false;

        // Create the cubemap texture
        GLuint cubeMapTextureID{0};
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubeMapTextureID);
        glTextureStorage2D(cubeMapTextureID, mipLevels, GL_SRGB8, renderTargetSize, renderTargetSize);

        createCubemapFaces(scene, cubeFace, resolveTarget, cubeMapTextureID);

        // Texture params
        glGenerateTextureMipmap(cubeMapTextureID);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Assign the texture
        auto &waterPlane = renderable.objectRef.get();
        textureManager_.setMaterial(waterPlane.getHandle(), CUBEMAP, cubeMapTextureID);
        renderable.properties.isVisible = true;
    }

    // Reset
    camera.resetFOV();
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateUBO(scene);

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::createCubemapFaces(Scene &scene, RenderTarget &msaaTarget, RenderTarget &blitTarget, GLuint texture)
{
    // For iterating cubemap faces
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    // Helper for rendering the scene
    auto renderFace = [&](RenderTarget &target, WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateUBO(scene);
        target.bind();
        target.clear();
        renderAllSceneObjects(scene);
    };

    const int renderTargetSize{msaaTarget.getWidth()};
    Camera &camera{scene.getCamera()};
    GLuint blitTargetTexture{blitTarget.getColorTextureID()};

    // Copy each MSAA direction into a cubemap face
    for (GLuint i{0}; i < 6; ++i)
    {
        msaaTarget.clear();
        blitTarget.clear();
        renderFace(msaaTarget, faces[i], camera);
        msaaTarget.blitTo(blitTarget);
        glCopyImageSubData(blitTargetTexture, GL_TEXTURE_2D, 0, 0, 0, 0, texture, GL_TEXTURE_CUBE_MAP, 0, 0, 0, i,
                           renderTargetSize, renderTargetSize, 1);
    }
}

void Renderer::populateSSBOs(Scene &scene, std::size_t totalInstanceCount)
{
    // Reserve max amount for matrices
    std::vector<glm::mat4> modelMatrices;
    modelMatrices.reserve(totalInstanceCount);
    instanceIndices_.reserve(totalInstanceCount);

    GLuint matricesOffset{0};
    instanceIndices_.clear();

    for (auto &[_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            auto &object{renderable.objectRef.get()};
            auto &instances{renderable.properties.instances};

            if (instances.empty())
                continue;

            GLuint numInstances{static_cast<GLuint>(instances.size())};

            renderable.properties.visibleInstances = numInstances;

            // Keep track of start position for each model's matrices in the SSBO. Before any culling there is a 1:1
            // mapping of the model matrices and indices
            modelBaseIndices_.emplace(object.getHandle(), ssboOffsets{matricesOffset, matricesOffset});

            for (std::size_t i{0}; i < instances.size(); ++i)
            {
                // Build up the vector of matrices to eventually pass to the SSBO
                modelMatrices.emplace_back(instances[i].transform.getModelMatrix());

                // Add each index as well (offset + index will give the proper model matrix)
                instanceIndices_.push_back(static_cast<GLuint>(i));
            }

            matricesOffset += numInstances;
        }

    glNamedBufferSubData(matricesSSBO_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(std::uint32_t), instanceIndices_.data());
}