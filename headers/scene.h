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

#define MAX_MODELS 100
#define MAX_SPACE_MODELS 10
#define MAX_SKYBOXES 5
#define MAX_GRASSES 10
#define MAX_PER_LIGHT_TYPE 5

//-----------------------------------------------------
// Any storage container (Models, lights, etc.) must guarantee that their
// addresses remain valid for the lifetime of the scene
//
// Scene is for fast access of static objects - dynamic model loading
// requires safer ownership model.
//-----------------------------------------------------

class Scene {
 public:
  Scene();
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&& other) noexcept;
  Scene& operator=(Scene&& other) noexcept;

  Camera camera;
  std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects();

  Transform& getObjectTransform(const std::string& key);
  Light& getNamedLight(const std::string& key);
  DirectionalLight& getGlobalLightSource();
  std::vector<PointLight>& getPointLights();

 protected:
  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  std::unordered_map<std::string, std::reference_wrapper<Transform>> namedTransforms;
  std::unordered_map<std::string, std::reference_wrapper<Light>> namedLights;

  std::vector<Model> models;
  std::vector<SpaceModel> spaceModels;
  std::vector<Skybox> skyboxes;
  std::vector<Grass> grasses;
  DirectionalLight globalLightSource;
  std::vector<PointLight> pointLights;

  virtual void addAllModelsToScene() = 0;
  virtual void addAllLightsToScene() = 0;
  virtual void setupCamera() = 0;
  
  void initializeScene();
  Model& loadModel(const std::filesystem::path& filepath);
  SpaceModel& loadSpaceModel(const std::filesystem::path& filepath);
  Skybox& loadSkybox(const std::vector<std::string>& filepaths);
  Grass& loadGrass(const std::filesystem::path& positionsPath,
                   const std::filesystem::path& texturePath, Grass::TuftPattern tuftPattern, float minSize,
                   float maxSize, unsigned int tuftDensity = 3,
                   unsigned int numPositions = 0);

  // Give a model a name if you want to reference its transform in Application's
  // onUpdate() loop
  template <typename T, typename U>
  void addObjectToScene(T& modelRef, ShaderType type, U&& transform,
                        const std::optional<std::string> name = std::nullopt) {
    sceneObjects[type].emplace_back(
        RenderableObject{std::ref(modelRef), std::forward<U>(transform)});

    if (name.has_value())
      namedTransforms.emplace(name.value(),
                              std::ref(sceneObjects[type].back().transform));
  }

  /**
   * Adds a light to the scene.  Optional: name the light to reference it
   * in application's onUpdate()
   * @param lightVec The vector to add the light to.
   * @param light The light object to insert.
   * @param name Optional name for later lookup.
   */
  template <typename LightType>
  void addLightToScene(std::vector<LightType>& lightVec, LightType&& light,
                       const std::optional<std::string> name = std::nullopt) {
    if (lightVec.size() >= MAX_PER_LIGHT_TYPE)
      throw std::runtime_error("Max number of lights reached");

    lightVec.emplace_back(std::forward<LightType>(light));

    if (name.has_value())
      namedLights.emplace(name.value(), std::ref(lightVec.back()));
  }
};