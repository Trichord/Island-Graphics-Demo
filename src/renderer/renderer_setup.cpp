#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "camera.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "render_target.h"
#include "renderable.h"
#include "scene.h"
#include "settings_window.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "texture_utilities.h"
#include "textures.h"
#include "transform.h"
#include "world_vectors.h"

void Renderer::loadInstancedData(Scene &scene)
{
    for (SceneEntry &sceneEntry : scene.getAllSceneEntries())
    {
        Renderable &renderable{sceneEntry.renderableRef.get()};

        if (auto instancedData = renderable.loadInstancedTransforms(); instancedData.has_value())
        {
            std::vector<BoundedTransform> &data{instancedData.value()};
            sceneEntry.transforms = std::move(data);
        }
    }
}

void Renderer::createBuffers(Scene &scene)
{
    // UBOs
    glCreateBuffers(1, &cameraUBO_);
    glNamedBufferStorage(cameraUBO_, sizeof(PackedShaderUniforms), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO_);

    glCreateBuffers(1, &shadowCascadesUBO_);
    glNamedBufferStorage(shadowCascadesUBO_, sizeof(PackedShadowCascadeData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, shadowCascadesUBO_);

    // Find total number of all instances across all scene objects
    std::size_t numInstances{0};
    GlobalPrimitiveStats stats;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        numInstances += entry.transforms.size();

        for (Primitive::Mesh &mesh : entry.renderableRef.get().getMeshes())
        {
            for (auto &primitive : mesh.primitives)
            {
                ++stats.primitives;
                stats.indices += primitive.getIndices().size();
                stats.vertices += primitive.getVertices().size();
            }
        }
    }

    // Instanced model matrices
    glCreateBuffers(1, &matricesSSBO_);
    glNamedBufferStorage(matricesSSBO_, numInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, matricesSSBO_);

    // Indices into the model matrices buffer
    glCreateBuffers(1, &indicesSSBO_);
    glNamedBufferStorage(indicesSSBO_, numInstances * sizeof(std::uint32_t), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indicesSSBO_);

    // Per-primitive data
    glCreateBuffers(1, &primitivesSSBO_);
    glNamedBufferStorage(primitivesSSBO_, stats.primitives * sizeof(PrimitiveData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, primitivesSSBO_);

    // Fill the SSBOs with all instanced model data
    createVAO(scene, stats);
    populateMatricesSSBO(numInstances, stats);

    // Metadata includes matrices, must come after matrices SSBO has been filled
    populateMetadataSSBO(stats);
}

void Renderer::populateMatricesSSBO(std::size_t totalInstanceCount, GlobalPrimitiveStats &stats)
{
    std::vector<glm::mat4> modelMatrices;

    instanceIndices_.reserve(totalInstanceCount);
    modelMatrices.reserve(totalInstanceCount);

    std::size_t matricesIndex{0};

    for (auto &[_, drawEntries] : drawEntriesByShader_)
    {
        for (auto &drawEntry : drawEntries)
        {
            Renderable &renderable{drawEntry.renderableRef.get()};
            std::vector<BoundedTransform> &instances{drawEntry.transformsRef.get()};

            if (instances.empty())
                continue;

            const GLuint numInstances{static_cast<GLuint>(instances.size())};

            // Before cubemaps are created, each index in the indices buffer maps 1:1 to a matrix in the matrices buffer
            // (nothing culled)
            drawEntry.indicesBufferIndex = static_cast<GLuint>(matricesIndex);
            matricesBufferIndexByHandle.emplace(renderable.getHandle(), static_cast<GLuint>(matricesIndex));

            for (std::size_t i{0}; i < instances.size(); ++i)
            {
                // Build up the vector of matrices to eventually pass to the SSBO
                modelMatrices.emplace_back(instances[i].transform.getModelMatrix());

                // Add each index as well (offset + index will give the proper model matrix)
                instanceIndices_.push_back(static_cast<GLuint>(i));
            }

            for (Primitive::Mesh &mesh : renderable.getMeshes())
            {
                for (Primitive &primitive : mesh.primitives)
                {
                    primitive.setMatricesBufferIndex(static_cast<int>(matricesIndex));
                }
            }

            matricesIndex += numInstances;
        }
    }

    glNamedBufferSubData(matricesSSBO_, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
    glNamedBufferSubData(indicesSSBO_, 0, instanceIndices_.size() * sizeof(std::uint32_t), instanceIndices_.data());
}

void Renderer::populateMetadataSSBO(GlobalPrimitiveStats &stats)
{
    std::size_t primitiveIndex{0};

    std::vector<PrimitiveData> primitiveData;
    primitiveData.resize(stats.primitives);

    for (auto &[_, drawEntries] : drawEntriesByShader_)
    {
        for (auto &drawEntry : drawEntries)
        {
            Renderable &renderable{drawEntry.renderableRef.get()};
            std::vector<BoundedTransform> &instances{drawEntry.transformsRef.get()};

            if (instances.empty())
                continue;

            // Materials - fix the data types here. It's a mess
            const std::vector<Material> &materials{textureManager_.getMaterials(renderable.getHandle())};

            for (Primitive::Mesh &meshes : renderable.getMeshes())
            {
                for (Primitive &primitive : meshes.primitives)
                {
                    std::size_t matIndex{primitive.getMaterialIndex()};

                    primitiveData[primitiveIndex].albedoArray = materials[matIndex].albedo.array;
                    primitiveData[primitiveIndex].albedoLayer = materials[matIndex].albedo.layer;

                    primitiveData[primitiveIndex].armArray = materials[matIndex].ARM.array;
                    primitiveData[primitiveIndex].armLayer = materials[matIndex].ARM.layer;

                    primitiveData[primitiveIndex].normalArray = materials[matIndex].normal.array;
                    primitiveData[primitiveIndex].normalLayer = materials[matIndex].normal.layer;

                    primitiveData[primitiveIndex].matricesBufferBaseIndex = primitive.getMatricesBufferIndex();

                    primitive.setMetadataBufferIndex(static_cast<GLuint>(primitiveIndex));
                    ++primitiveIndex;
                }
            }
        }
    }

    glNamedBufferSubData(primitivesSSBO_, 0, primitiveData.size() * sizeof(PrimitiveData), primitiveData.data());
}

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */
void Renderer::createVAO(Scene &scene, GlobalPrimitiveStats stats)
{
    glCreateVertexArrays(1, &globalVAO_);
    glBindVertexArray(globalVAO_);

    glEnableVertexArrayAttrib(globalVAO_, 0);
    glEnableVertexArrayAttrib(globalVAO_, 1);
    glEnableVertexArrayAttrib(globalVAO_, 2);
    glEnableVertexArrayAttrib(globalVAO_, 3);

    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    std::size_t VAOIndex{0};
    std::size_t EBOIndex{0};

    vertices.reserve(stats.vertices);
    indices.reserve(stats.indices);

    for (auto &[_, entries] : drawEntriesByShader_)
    {
        for (DrawEntry &entry : entries)
        {
            Renderable &renderable{entry.renderableRef.get()};
            std::span<Primitive::Mesh> meshes{renderable.getMeshes()};

            ModelHandle handle{renderable.getHandle()};
            const std::vector<Material> &materials{textureManager_.getMaterials(handle)};

            for (Primitive::Mesh &mesh : meshes)
            {
                for (Primitive &primitive : mesh.primitives)
                {
                    std::span<const Primitive::Vertex> objectVertices{primitive.getVertices()};
                    std::span<const std::uint32_t> objectIndices{primitive.getIndices()};

                    std::copy(objectVertices.begin(), objectVertices.end(), std::back_inserter(vertices));
                    std::copy(objectIndices.begin(), objectIndices.end(), std::back_inserter(indices));

                    primitive.setVAOIndex(static_cast<GLuint>(VAOIndex));
                    primitive.setEBOIndex(static_cast<GLuint>(EBOIndex));

                    VAOIndex += primitive.getVertices().size();
                    EBOIndex += primitive.getIndices().size();
                }
            }
        }
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

    if (!drawEntriesByShader_.contains(SKYBOX) || !drawEntriesByShader_.contains(OCEAN))
        return;

    auto &skyboxes = drawEntriesByShader_[SKYBOX];
    auto &oceans = drawEntriesByShader_[OCEAN];

    if (skyboxes.empty() || oceans.empty())
        return;

    auto &skybox = skyboxes[0].renderableRef.get();

    for (int i{0}; i < oceans.size(); ++i)
    {
        auto &ocean = oceans[i].renderableRef.get();

        textureManager_.borrowCubemaps(ocean.getHandle(), skybox.getHandle());
    }
}

void Renderer::createCubemaps(Scene &scene)
{
    using enum ShaderType;

    auto waterIter = drawEntriesByShader_.find(WATER);

    if (waterIter == drawEntriesByShader_.end() || waterIter->second.empty())
        return;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        entry.renderableRef.get().setMeshVisibility(true);
    }

    auto &waters{waterIter->second};

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

    for (auto &drawEntry : waters)
    {
        std::vector<BoundedTransform> &instances{drawEntry.transformsRef.get()};

        if (instances.empty())
            continue;

        // Set the camera in the center of the water plane. Water should not be instanced
        Transform &transform{instances[0].transform};
        camera.setPosition(transform.getPosition());

        // Hide self
        drawEntry.isVisible = false;

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
        auto &waterPlane = drawEntry.renderableRef.get();
        textureManager_.setCubemap(waterPlane.getHandle(), cubeMapTextureID);
        drawEntry.isVisible = true;
    }

    // Reset
    camera.setFOV(originalFOV);
    camera.setAspectRatio(originalAspect);
    camera.setPosition(originalPos);
    camera.setPitch(originalPitch);
    camera.setYaw(originalYaw);
    updateCameraUBO(scene);

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