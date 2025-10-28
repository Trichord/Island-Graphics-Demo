#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <utility>
#include <memory>
#include <stdexcept>
#include <filesystem>

#include "instanced_model.h"
#include "model.h"
#include "space_model.h"
#include "skybox.h"
#include "camera.h"
#include "renderable.h"
#include "shader_types.h"
#include "transform.h"
#include "point_light.h"
#include "directional_light.h"
#include "grass.h"

//-----------------------------------------------------
// Any storage container (Models, lights, etc.) must guarantee that their
// addresses remain valid for the lifetime of the scene
//
// Scene is for fast access of static objects - dynamic model loading
// requires safer ownership model.
//-----------------------------------------------------

namespace sceneMaxObjects {
inline constexpr int maxModels{50};
inline constexpr int maxSpaceModels{10};
inline constexpr int maxSkyboxes{10};
inline constexpr int maxGrasses{10};
inline constexpr int maxLights{5};
inline constexpr int maxInstancedModels{25};
}  // namespace sceneMaxObjects

class Scene {
 public:
  Scene();
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&& other) noexcept;
  Scene& operator=(Scene&& other) noexcept;

  Camera camera;

  std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects();
  std::vector<PointLight>& getPointLights();
  Transform& getObjectTransform(const std::string& key);
  Light& getNamedLight(const std::string& key);
  DirectionalLight& getGlobalLightSource();

 protected:
  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  std::unordered_map<std::string, std::reference_wrapper<Transform>>
      namedTransforms;
  std::unordered_map<std::string, std::reference_wrapper<Light>> namedLights;
  std::vector<Model> models;
  std::vector<SpaceModel> spaceModels;
  std::vector<Skybox> skyboxes;
  std::vector<Grass> grasses;
  std::vector<InstancedModel> instancedModels;
  std::vector<PointLight> pointLights;

  DirectionalLight globalLightSource;

  virtual void addAllModelsToScene() = 0;
  virtual void addAllLightsToScene() = 0;
  virtual void setupCamera() = 0;

  void initializeScene();
  Model& loadModel(const std::filesystem::path& filepath);
  SpaceModel& loadSpaceModel(
      const std::filesystem::path& modelPath,
      const std::vector<std::filesystem::path>& imagePaths);
  Skybox& loadSkybox(const std::vector<std::filesystem::path>& filePaths);
  Grass& loadGrass(const std::filesystem::path& positionsPath,
                   const std::filesystem::path& texturePath,
                   Grass::TuftPattern tuftPattern, float minSize, float maxSize,
                   unsigned int tuftDensity = 3, unsigned int numPositions = 0);
  InstancedModel& loadInstancedModel(const std::filesystem::path& modelPath,
                                     const std::filesystem::path& positionsPath,
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
  template <typename T, typename U>
  void addObjectToScene(T& modelRef, ShaderType shaderType, U&& transform,
                        const std::optional<std::string> name = std::nullopt) {
    sceneObjects[shaderType].emplace_back(
        RenderableObject{std::ref(modelRef), std::forward<U>(transform)});

    if (name.has_value())
      namedTransforms.emplace(name.value(), std::ref(sceneObjects[shaderType].back().transform));
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