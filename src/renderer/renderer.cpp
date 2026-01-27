#include "renderer.h"

#include <array>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "camera.h"
#include "config_window.h"
#include "directional_light.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "material_slots.h"
#include "point_light.h"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "transform.h"

// Normalized wind direction, east southeast with a slight downward angle
constexpr float windStrength{0.07f};
constexpr glm::vec3 windDir{0.892079f, -0.101373f, 0.440362f};

// Bright midday sun
constexpr glm::vec3 ambientLight{0.15f};
constexpr glm::vec3 lightColor{1.0f, 0.95f, 0.9f};

// Scale mipmaps and alpha cutoff for foliage
constexpr float mipScale{0.25f};
constexpr float alphaCutoff{0.5f};

// Culling range for grass
constexpr float grassCullDistance{0.4f};

Renderer::Renderer(const ConfigWindow::GraphicsSettings& settings)
    : graphicsSettings{settings}, glState{settings.msaa != 0} {
  setupTextureManager();
  setupUBO();
  loadShaders();

  for (auto& [_, shader] : shaders) setupShader(shader);
}

Renderer::Renderer(Renderer&& other) noexcept
    : UBO{other.UBO},
      uboData{std::move(other.uboData)},
      shaders{std::move(other.shaders)},
      graphicsSettings{other.graphicsSettings},
      textureManager{std::move(other.textureManager)},
      glState{std::move(other.glState)} {
  other.UBO = 0;
}

void Renderer::renderScene(Scene& scene) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  updateShaderUniforms(scene);

  resetVisibility(scene);
  cullRenderables(scene);

  renderPrePass(scene);
  renderAllSceneObjects(scene);
  renderSkybox(scene);
}

void Renderer::updateUBO(Scene& scene) {
  uboData.viewMatrix = scene.camera.getViewMatrix();
  uboData.viewProjectionMatrix = scene.camera.getViewProjectionMatrix();
  uboData.cameraPosition = glm::vec4{scene.camera.getPosition(), 0.0f};
 
  void* uboPointer{
      glMapNamedBufferRange(UBO, 0, sizeof(PackedUBOData),
                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)};

  std::memcpy(uboPointer, &uboData, sizeof(PackedUBOData));

  glUnmapNamedBuffer(UBO);
}

void Renderer::setupUBO() {
  glCreateBuffers(1, &UBO);

  glNamedBufferStorage(UBO, sizeof(PackedUBOData), nullptr,
                       GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
}

void Renderer::setupTextureManager() {
  if (graphicsSettings.anisotropicFiltering)
    textureManager.setAnisoLevel(
        static_cast<float>(graphicsSettings.anisotropicFiltering));
}

void Renderer::loadShaders() {
  using enum ShaderType;

  if (graphicsSettings.msaa) {
    shaders.emplace(FLOWERS, Shader{"./shaders/flowers.vert",
                                    "./shaders/flowers_msaa.frag"});
    shaders.emplace(
        TREES, Shader{"./shaders/trees.vert", "./shaders/trees_msaa.frag"});
  } else {
    shaders.emplace(FLOWERS,
                    Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
    shaders.emplace(TREES,
                    Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
  }

  shaders.emplace(POINT_PBR, Shader{"./shaders/point_PBR.vert",
                                    "./shaders/point_PBR.frag"});
  shaders.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert",
                                          "./shaders/directional_PBR.frag"});
  shaders.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert",
                                     "./shaders/reflective.frag"});
  shaders.emplace(SKYBOX,
                  Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
}

void Renderer::setupShader(Shader& shader) {
  using enum MaterialSlot;

  shader.setUniformBlock("matrices", 0);

  shader.setInt("albedoMap", ALBEDO);
  shader.setInt("occlusionRoughMetalMap", ARM);
  shader.setInt("normalMap", NORMAL);
  shader.setInt("cubeMap", CUBEMAP);

  shader.setVec3("ambientLight", ambientLight);
  shader.setVec3("lightColor", lightColor);

  shader.setFloat("windStrength", windStrength);
  shader.setVec3("windDirection", windDir);

  shader.setFloat("mipScale", mipScale);
  shader.setFloat("alphaCutoff", alphaCutoff);

  shader.setFloat("gamma", graphicsSettings.gamma);
}

void Renderer::updateShaderUniforms(Scene& scene) {
  DirectionalLight& globalLight{scene.getGlobalLightSource()};
  bool dirUpdated{globalLight.directionHasUpdated()};

  std::vector<PointLight>& pointLights{scene.getPointLights()};

  bool posUpdated{false};
  glm::vec3 pointPos{0.0f};
  if (!pointLights.empty()) {
    pointPos = pointLights[0].getPosition();
    posUpdated = pointLights[0].positionHasUpdated();
  }

  // Set uniforms
  for (auto& [_, shader] : shaders) {
    using enum ShaderType;
    if (dirUpdated) shader.setVec3("lightDir", globalLight.getDirection());
    if (posUpdated) shader.setVec3("lightPosition", pointPos);

    shader.setFloat("time", (float)glfwGetTime());
    shader.setMat4("viewProjectionNoTranslation",
                   scene.camera.getProjectionMatrix() *
                       scene.camera.getViewMatrixNoTranslation());
  }
}

void Renderer::renderPrePass(Scene& scene) {}

void Renderer::cullRenderables(Scene& scene) {
  const std::array<glm::vec4, 6>& frustumPlanes{
      scene.camera.getFrustumPlanes()};

  for (auto& [shaderType, objectList] : scene.getSceneObjects())
    for (auto& object : objectList) {
      using enum CullStrategy;

      CullStrategy strat{object.cullStrategy};

      switch (strat) {
        case MODEL:
          cullModel(object, frustumPlanes);
          break;

        case BY_MESH:
          cullByMesh(object, frustumPlanes);
          break;

        case INSTANCED:
          cullInstanced(object, scene.camera.getPosition(), frustumPlanes,
                        false);
          break;

        case INSTANCED_DISTANCE:
          cullInstanced(object, scene.camera.getPosition(), frustumPlanes,
                        true);
          break;

        case NONE:
          break;

        default:
          cullModel(object, frustumPlanes);
      }
    }
}

void Renderer::bindTextures(const Material& materials) {
  using enum MaterialSlot;

  if (materials.albedo) glState.bindTexture(ALBEDO, materials.albedo);

  if (materials.occlusionRoughnessMetallic)
    glState.bindTexture(ARM, materials.occlusionRoughnessMetallic);

  if (materials.normal) glState.bindTexture(NORMAL, materials.normal);

  if (materials.cubemap) glState.bindTexture(CUBEMAP, materials.cubemap);
}

TextureManager& Renderer::getTextureManager() { return textureManager; }

void Renderer::cullModel(Renderable& renderable,
                         const std::array<glm::vec4, 6>& frustumPlanes) {
  const glm::mat4& modelMatrix = renderable.transform.getModelMatrix();
  auto& object = renderable.object.get();
  const AABB& aabb = object.getAABB();

  renderable.isVisible = aabb.intersects(frustumPlanes, modelMatrix);
}

void Renderer::cullInstanced(Renderable& renderable, const glm::vec3& cameraPos,
                             const std::array<glm::vec4, 6>& frustumPlanes,
                             bool cullByDistance) {
  float nearDistance{frustumPlanes[4].w};
  float farDistance{frustumPlanes[5].w};
  bool anyVisible{false};

  const float cullDistance{nearDistance +
                           (farDistance - nearDistance) * grassCullDistance};

  auto& object = renderable.object.get();
  std::span<InstanceData> instances = object.getInstanceData();

  for (auto& instance : instances) {
    const float objToCamera{glm::length(cameraPos - instance.position)};

    // Outside of distance based culling
    if (cullByDistance && objToCamera > cullDistance) {
      instance.isVisible = false;
      continue;
    }

    instance.isVisible = instance.worldAABB.intersectsWorldSpace(frustumPlanes);
    if (instance.isVisible) {
      anyVisible = true;
      double xDist{instance.position.x - cameraPos.x};
      double zDist{instance.position.z - cameraPos.z};
      instance.sortKey = xDist * xDist + zDist * zDist;
      renderable.isVisible = true;
    }
  }

  renderable.isVisible = anyVisible;

  if (anyVisible) object.sortInstances();
}

void Renderer::cullByMesh(Renderable& renderable,
                          const std::array<glm::vec4, 6>& frustumPlanes) {
  bool visible{false};

  for (auto& mesh : renderable.object.get().getMeshes()) {
    for (const auto& primitive : mesh.primitives) {
      if (primitive.getAABB().intersects(
              frustumPlanes, renderable.transform.getModelMatrix())) {
        mesh.isVisible = true;
        visible = true;
        break;
      } else
        mesh.isVisible = false;
    }
  }

  renderable.isVisible = visible;
}

void Renderer::resetVisibility(Scene& scene) {
  for (auto& [_, objectList] : scene.getSceneObjects())
    for (auto& renderable : objectList) {
      renderable.isVisible = true;
      renderable.object.get().resetVisibility();
    }
}

void Renderer::renderSkybox(Scene& scene) {
  using enum ShaderType;
  auto& skyboxes = scene.getSceneObjects()[SKYBOX];

  if (skyboxes.empty()) return;

  glDepthFunc(GL_LEQUAL);

  for (auto& renderable : skyboxes) {
    auto& skybox = renderable.object.get();

    const std::vector<Material>& materials{
        textureManager.acquireUniqueTextureSet(skybox.getModelHandle())};

    if (!materials.empty()) bindTextures(materials[0]);

    for (auto& mesh : skybox.getMeshes())
      for (auto& primitive : mesh.primitives) {
        glBindVertexArray(primitive.getVAO());
        glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(),
                       GL_UNSIGNED_INT, 0);
      }
  }

  glDepthFunc(GL_LESS);
}

void Renderer::renderAllSceneObjects(Scene& scene) {
  using enum ShaderType;

  // For each shader type in the scene
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    if (objectList.empty()) continue;

    Shader& shader{shaders.at(shaderType)};
    shader.use();

    // For each object of that shader type
    for (auto& renderable : objectList) {
      if (!renderable.isVisible) continue;

      glm::mat4 modelMatrix{renderable.transform.getModelMatrix()};
      glm::mat3 normalMatrix{renderable.transform.getNormalMatrix()};

      shader.setMat4("modelMatrix", modelMatrix);
      shader.setMat3("normalMatrix", normalMatrix);

      // Draw it
      auto& object = renderable.object.get();
      renderObject(object);
    }
  }
}

void Renderer::renderObject(RenderableObject& object) {
  std::size_t lastMaterialIndex{0};
  bool materialBound{false};
  const auto materials{
      textureManager.acquireUniqueTextureSet(object.getModelHandle())};

  for (const Primitive::Mesh& mesh : object.getMeshes()) {
    if (!mesh.isVisible) continue;

    for (const Primitive& primitive : mesh.primitives) {
      glState.setAlphaMode(primitive.getAlphaMode() !=
                           fastgltf::AlphaMode::Opaque);

      if (primitive.materialsExist()) {
        std::size_t materialIndex{primitive.getMaterialIndex()};

        // Switch materials
        if (materialIndex != lastMaterialIndex || !materialBound) {
          lastMaterialIndex = materialIndex;
          materialBound = true;
          bindTextures(materials[materialIndex]);
        }
      }

      glBindVertexArray(primitive.getVAO());

      if (object.getInstanceData().empty()) {
        glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(),
                       GL_UNSIGNED_INT, 0);
      } else
        glDrawElementsInstanced(
            GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0,
            static_cast<GLsizei>(object.getInstanceCount()));
    }
  }
  glState.setAlphaMode(false);
}