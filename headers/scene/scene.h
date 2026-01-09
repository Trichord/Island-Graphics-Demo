#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "instanced_model.h"
#include "model.h"
#include "point_light.h"
#include "renderable.h"
#include "shader_types.h"
#include "skybox.h"
#include "transform.h"
#include "texture_manager.h"
#include "cube_mapped_model.h"

/*
 * Any storage container (Models, lights, etc.) must guarantee that their
 * addresses remain valid for the lifetime of the scene
 *
 * Scene is for fast access of static objects - dynamic model loading requires
 * safer ownership model.
 */

namespace sceneMaxObjects {
inline constexpr int maxModels{50};
inline constexpr int maxCubeMappedModels{10};
inline constexpr int maxSkyboxes{10};
inline constexpr int maxQuadModels{10};
inline constexpr int maxLights{5};
inline constexpr int maxInstancedModels{25};
}  // namespace sceneMaxObjects

class Scene {
 public:
  Scene(TextureManager& textureManager);
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&& other) noexcept = default;
  Scene& operator=(Scene&& other) noexcept = default;

  Camera camera;

  std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects();
  std::vector<PointLight>& getPointLights();
  Transform& getObjectTransform(const std::string& key);
  Light& getNamedLight(const std::string& key);
  DirectionalLight& getGlobalLightSource();

 protected:
  TextureManager& textureManager;

  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  std::unordered_map<std::string, std::reference_wrapper<Transform>>
      namedTransforms;
  std::unordered_map<std::string, std::reference_wrapper<Light>> namedLights;
  std::vector<Model> models;
  std::vector<CubeMappedModel> cubeMappedModels;
  std::vector<Skybox> skyboxes;
  std::vector<InstancedModel> instancedModels;
  std::vector<PointLight> pointLights;

  DirectionalLight globalLightSource;

  virtual void addAllModelsToScene() = 0;
  virtual void addAllLightsToScene() = 0;
  virtual void setupCamera() = 0;

  void initializeScene();
  Model& loadModel(const std::filesystem::path& filepath);
  Model& loadModel(const Model& borrowTarget,
                   const std::filesystem::path& filepath);
  CubeMappedModel& loadCubeMappedModel(
      const std::filesystem::path& modelPath,
      const std::vector<std::filesystem::path>& imagePaths);
  CubeMappedModel& loadCubeMappedModel(
      const CubeMappedModel& borrowTarget,
      const std::filesystem::path& modelPath,
      const std::vector<std::filesystem::path>& imagePaths);
  Skybox& loadSkybox(const std::vector<std::filesystem::path>& filePaths);
  Skybox& loadSkybox(const Skybox& borrowTarget,
                     const std::vector<std::filesystem::path>& filePaths);
  InstancedModel& loadInstancedModel(const std::filesystem::path& modelPath,
                                     const std::filesystem::path& positionsPath,
                                     float minSize = 1.0f, float maxSize = 1.0f,
                                     unsigned int numPositions = 0);
  InstancedModel& loadInstancedModel(const InstancedModel& borrowTarget, const std::filesystem::path& modelPath,
                                     const std::filesystem::path& positionsPath,
                                     float minSize = 1.0f, float maxSize = 1.0f,
                                     unsigned int numPositions = 0);

  /**
   * Adds an object to the scene.  Optional: give the object a name to reference
   * it in Application's onUpdate()
   * @param modelRef A reference to the model after its been loaded
   * using Scene's loading functions (e.g. loadModel, loadSkybox)
   * @param shaderType The shader this model will be rendered with
   * @param transform The position / rotation / scale transform applied to the
   * object
   */
  template <typename T>
  void addObjectToScene(T& modelRef, ShaderType shaderType,
                        Transform&& transform = Transform{},
                        const std::optional<std::string> name = std::nullopt) {
    sceneObjects[shaderType].emplace_back(
        RenderableObject{std::ref(modelRef), transform});

    if (name.has_value())
      namedTransforms.emplace(
          name.value(), std::ref(sceneObjects[shaderType].back().transform));
  }

  /**
   * Adds a light to the scene.  Optional: name the light to reference it
   * in Application's onUpdate()
   * @param lightVec The vector to add the light to.
   * @param light The light object to insert.
   * @param name Optional name for later lookup.
   */
  template <typename LightType>
  void addLightToScene(std::vector<LightType>& lightVec, LightType&& light,
                       const std::optional<std::string> name = std::nullopt) {
    if (lightVec.size() >= sceneMaxObjects::maxLights)
      throw std::runtime_error("Max number of lights reached");

    lightVec.emplace_back(std::forward<LightType>(light));

    if (name.has_value())
      namedLights.emplace(name.value(), std::ref(lightVec.back()));
  }
};