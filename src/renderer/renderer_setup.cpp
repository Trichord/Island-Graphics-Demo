#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include "camera.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "model.h"
#include "primitive.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"
#include "world_vectors.h"

#include <iostream>

Renderer::SceneTotals Renderer::getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.transforms.size()};
        totals.matrices += numTransforms;
        totals.visibleInstances += numTransforms * entry.modelRef.get().getPrimitives().size();

        for (Primitive &primitive : entry.modelRef.get().getPrimitives())
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();
        }
    }

    return totals;
}

void Renderer::createBuffers(Scene &scene)
{
    SceneTotals totals{getSceneTotals(scene)};
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(drawEntries_.size());

    // UBOs
    glCreateBuffers(1, &cameraUBO_);
    glNamedBufferStorage(cameraUBO_, sizeof(PackedShaderUniforms), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO_);

    glCreateBuffers(1, &shadowCascadesUBO_);
    glNamedBufferStorage(shadowCascadesUBO_, sizeof(PackedShadowCascadeData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, shadowCascadesUBO_);

    // Instanced model matrices
    glCreateBuffers(1, &matricesSSBO_);
    glNamedBufferStorage(matricesSSBO_, totals.matrices * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, matricesSSBO_);

    // Indices into the model matrices buffer. Each primitive has its own set of visible indicies, with the number of
    // model matrices being the upper bound
    glCreateBuffers(1, &visibleIndicesSSBO_);
    glNamedBufferStorage(visibleIndicesSSBO_, totals.visibleInstances * sizeof(std::uint32_t), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, visibleIndicesSSBO_);

    // Per-primitive data
    glCreateBuffers(1, &primitivesSSBO_);
    glNamedBufferStorage(primitivesSSBO_, drawEntries_.size() * sizeof(PrimitiveMetadata), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, primitivesSSBO_);

    // Draw command buffer
    glCreateBuffers(1, &drawCommandBuffer_);
    glNamedBufferStorage(drawCommandBuffer_, drawEntries_.size() * sizeof(DrawElementsIndirectCommand), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, drawCommandBuffer_);

    createVAO(scene, totals);
    populateMatricesSSBO(scene, metadata, totals);
    populateVisibleIndicesBuffer(metadata, totals);
    populateMetadataMaterials(scene, metadata);

    glNamedBufferSubData(primitivesSSBO_, 0, metadata.size() * sizeof(PrimitiveMetadata), metadata.data());
}

void Renderer::populateMatricesSSBO(Scene &scene, std::vector<PrimitiveMetadata> &metadata, SceneTotals &stats)
{
    std::uint32_t matricesIndex{0};
    std::vector<glm::mat4> modelMatrices;
    modelMatrices.reserve(stats.matrices);

    for (SceneEntry &sceneEntry : scene.getAllSceneEntries())
    {
        ModelHandle handle{sceneEntry.modelRef.get().getHandle()};

        for (Transform &transform : sceneEntry.transforms)
        {
            modelMatrices.emplace_back(transform.getModelMatrix());
        }

        for (std::uint32_t index : drawEntriesByHandle_.at(handle))
        {
            drawEntries_[index].matricesBufferBaseIndex = matricesIndex;
            metadata[index].matricesBufferBaseIndex = matricesIndex;
        }

        matricesIndex += static_cast<std::uint32_t>(sceneEntry.transforms.size());
    }

    glNamedBufferSubData(matricesSSBO_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
}

void Renderer::populateMetadataMaterials(Scene &scene, std::vector<PrimitiveMetadata> &metadata)
{
    // Each draw entry (primitive) gets its own section of the visibility buffer sized to the underlying model's number
    // of instances
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(drawEntries_.size()); ++i)
    {
        DrawEntry &drawEntry{drawEntries_[i]};
        PrimitiveMetadata &metaEntry{metadata[i]};

        const int matIndex{drawEntry.primitive->materialIndex};
        const std::vector<Material> &materials{textureManager_.getMaterials(drawEntry.handle)};
        const Material &material{materials[matIndex]};

        metaEntry.albedoArray = material.albedo.array;
        metaEntry.albedoLayer = material.albedo.layer;

        metaEntry.armArray = material.ARM.array;
        metaEntry.armLayer = material.ARM.layer;

        metaEntry.normalArray = material.normal.array;
        metaEntry.normalLayer = material.normal.layer;

        // primitiveData[primitiveIndex].cubemapArray = materials[matIndex].cubemap.array;
        // primitiveData[primitiveIndex].cubemapLayer = materials[matIndex].cubemap.layer; 
    }
}

void Renderer::populateVisibleIndicesBuffer(std::vector<PrimitiveMetadata> &metadata, SceneTotals &stats)
{
    std::uint32_t visibleIndex{0};
    visibleMatrices_.reserve(stats.visibleInstances);

    // For the cubemaps, push every transform index so everything is visible
    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        std::vector<Transform> &transforms{*drawEntries_[i].transforms};

        for (std::size_t j{0}; j < transforms.size(); ++j)
        {
            visibleMatrices_.emplace_back(static_cast<std::uint32_t>(j));
        }

        drawEntries_[i].visibleIndiciesBufferBaseIndex = visibleIndex;
        metadata[i].visibleInstancesBufferBaseIndex = visibleIndex;

        visibleIndex += static_cast<uint32_t>(transforms.size());
    }

    glNamedBufferSubData(visibleIndicesSSBO_, 0, visibleMatrices_.size() * sizeof(std::uint32_t),
                         visibleMatrices_.data());
}

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */
void Renderer::createVAO(Scene &scene, SceneTotals stats)
{
    glCreateVertexArrays(1, &globalVAO_);
    glBindVertexArray(globalVAO_);

    glEnableVertexArrayAttrib(globalVAO_, 0);
    glEnableVertexArrayAttrib(globalVAO_, 1);
    glEnableVertexArrayAttrib(globalVAO_, 2);
    glEnableVertexArrayAttrib(globalVAO_, 3);

    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    std::uint32_t vaoIndex{0};
    std::uint32_t eboIndex{0};

    vertices.reserve(stats.vertices);
    indices.reserve(stats.indices);

    for (DrawEntry &entry : drawEntries_)
    {
        std::copy(entry.primitive->vertices.begin(), entry.primitive->vertices.end(), std::back_inserter(vertices));
        std::copy(entry.primitive->indices.begin(), entry.primitive->indices.end(), std::back_inserter(indices));

        entry.vaoBaseIndex = vaoIndex;
        entry.eboBaseIndex = eboIndex;

        vaoIndex += static_cast<std::uint32_t>(entry.primitive->vertices.size());
        eboIndex += static_cast<std::uint32_t>(entry.primitive->indices.size());
    }

    glCreateBuffers(1, &globalVBO_);
    glNamedBufferStorage(globalVBO_, vertices.size() * sizeof(Primitive::Vertex), vertices.data(), 0);

    glCreateBuffers(1, &globalEBO_);
    glNamedBufferStorage(globalEBO_, indices.size() * sizeof(std::uint32_t), indices.data(), 0);

    glVertexArrayVertexBuffer(globalVAO_, 0, globalVBO_, 0, sizeof(Primitive::Vertex));

    glVertexArrayElementBuffer(globalVAO_, globalEBO_);

    glVertexArrayAttribFormat(globalVAO_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, position));
    glVertexArrayAttribFormat(globalVAO_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, normal));
    glVertexArrayAttribFormat(globalVAO_, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, tangent));
    glVertexArrayAttribFormat(globalVAO_, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Primitive::Vertex, texCoords));

    glVertexArrayAttribBinding(globalVAO_, 0, 0);
    glVertexArrayAttribBinding(globalVAO_, 1, 0);
    glVertexArrayAttribBinding(globalVAO_, 2, 0);
    glVertexArrayAttribBinding(globalVAO_, 3, 0);
}

void Renderer::borrowSkybox()
{
    using enum ShaderType;

    auto skyboxIter = std::ranges::lower_bound(drawEntries_, ShaderType::skybox, {}, &DrawEntry::shaderType);
    auto oceanIter = std::ranges::lower_bound(drawEntries_, ShaderType::ocean, {}, &DrawEntry::shaderType);

    if (skyboxIter == drawEntries_.end() || oceanIter == drawEntries_.end())
        return;

    ModelHandle skyboxHandle{skyboxIter->handle};

    while (oceanIter->shaderType == ShaderType::ocean)
    {
        textureManager_.borrowCubemaps(oceanIter->handle, skyboxHandle);
        ++oceanIter;
    }
}

void Renderer::createCubemaps(Scene &scene)
{
    using enum ShaderType;

    auto waterIter = std::ranges::lower_bound(drawEntries_, water, {}, &DrawEntry::shaderType);

    if (waterIter == drawEntries_.end())
        return;

    // Copy the start of the water iterator to renable visibility
    auto waterEnableIter = waterIter;

    while (waterIter->shaderType == water)
    {
        waterIter->anyVisible = false;
        ++waterIter;
    }

    Camera &camera{scene.getCamera()};

    // Half size render targets for water reflections
    int renderTargetSize{std::max(graphicsSettings_.screenWidth, graphicsSettings_.screenHeight) / 2};
    int mipLevels = static_cast<int>(std::floor(std::log2(renderTargetSize)) + 1);

    // Save camera settings before we make it bounce around the scene
    const glm::vec3 originalPos{camera.getPosition()};
    const float originalPitch{camera.getPitch()};
    const float originalYaw{camera.getYaw()};
    const float originalFOV{camera.getFOV()};
    const float originalAspect{camera.getAspectRatio()};

    // 90 degrees for cubemap
    camera.setFOV(90.0f);
    camera.setAspectRatio(1.0f);

    glViewport(0, 0, renderTargetSize, renderTargetSize);

    // Water should not be instanced because of the 1 cubemap per model. By walking the scene, we ensure each model gets
    // one cubemap
    for (SceneEntry entry : scene.getAllSceneEntries())
    {
        if (entry.shaderType != water)
            continue;

        Transform &transform{entry.transforms.front()};

        // Set the camera in the center of the water plane
        camera.setPosition(transform.getPosition());

        // Create the cubemap texture
        GLuint cubeMapTextureID{0};
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubeMapTextureID);
        glTextureStorage2D(cubeMapTextureID, mipLevels, GL_SRGB8, renderTargetSize, renderTargetSize);

        createCubemapFaces(scene, renderTargetSize, cubeMapTextureID);

        // Texture params
        glGenerateTextureMipmap(cubeMapTextureID);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(cubeMapTextureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Assign the texture
        auto &waterPlane = entry.modelRef.get();
        textureManager_.setCubemap(waterPlane.getHandle(), cubeMapTextureID);
    }

    // Reset
    camera.setFOV(originalFOV);
    camera.setAspectRatio(originalAspect);
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateCameraUBO(scene);

    while (waterEnableIter->shaderType == water)
    {
        waterEnableIter->anyVisible = true;
        ++waterEnableIter;
    }

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, graphicsSettings_.screenWidth, graphicsSettings_.screenHeight);
}

void Renderer::createCubemapFaces(Scene &scene, int targetResolution, GLuint texture)
{
    // For iterating cubemap faces
    constexpr std::array<WorldOrientation, 6> faces{WorldOrientation::RIGHT, WorldOrientation::LEFT,
                                                    WorldOrientation::UP,    WorldOrientation::DOWN,
                                                    WorldOrientation::BACK,  WorldOrientation::FRONT};

    // Helper for rendering the scene
    auto renderFace = [&](RenderTarget &target, WorldOrientation orientation, Camera &camera) {
        camera.setWorldOrientation(orientation);
        updateCameraUBO(scene);
        target.clear();
        renderAllSceneObjects();
    };

    RenderTarget cubeFace(targetResolution, targetResolution,
                          {graphicsSettings_.msaa, RenderTarget::DepthType::RBO, GL_SRGB8, GL_DEPTH_COMPONENT24});
    RenderTarget blitTarget(targetResolution, targetResolution, {0, RenderTarget::DepthType::NONE, GL_SRGB8, GL_NONE});

    Camera &camera{scene.getCamera()};
    const int renderTargetSize{cubeFace.getWidth()};
    const GLuint blitTargetTexture{blitTarget.getColorTextureID()};

    cubeFace.bind();

    // Copy each MSAA direction into a cubemap face
    for (GLuint i{0}; i < 6; ++i)
    {
        cubeFace.clear();
        blitTarget.clear();
        renderFace(cubeFace, faces[i], camera);
        cubeFace.blitTo(blitTarget);
        glCopyImageSubData(blitTargetTexture, GL_TEXTURE_2D, 0, 0, 0, 0, texture, GL_TEXTURE_CUBE_MAP, 0, 0, 0, i,
                           renderTargetSize, renderTargetSize, 1);
    }

    cubeFace.unbind();
}