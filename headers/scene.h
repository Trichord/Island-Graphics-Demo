#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <string>
#include <optional>
#include <utility>
#include <memory>

#include "model.h"
#include "space_model.h"
#include "skybox.h"
#include "camera.h"
#include "renderable.h"
#include "shader_types.h"
#include "transform.h"
#include "point_light.h"
#include "directional_light.h"

//-----------------------------------------------------
// Any container storing Model or Light instances must guarantee that their
// addresses remain valid for the lifetime of the scene
//
// If using std::vector, you MUST call reserve()
// before inserting and NEVER modify the vector afterward — any reallocation
// will invalidate references and cause undefined behavior.
//
// Scene is for fast access of static objects - dynamic model loading
// requires safer ownership model.
//-----------------------------------------------------

class Scene {
 public:
  Scene() = default;
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&& other) noexcept;
  Scene& operator=(Scene&& other) noexcept;

  Camera camera;
  std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects();

  Transform& getObjectTransform(const std::string& key);
  Light& getNamedLight(const std::string& key);
  std::vector<DirectionalLight>& getDirectionalLights();
  std::vector<PointLight>& getPointLights();

 protected:
  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  std::map<std::string, std::reference_wrapper<Transform>> objectTransforms;
  std::map<std::string, std::reference_wrapper<Light>> namedLights;

  std::vector<Model> models;
  std::vector<SpaceModel> spaceModels;
  std::vector<Skybox> skyboxes;
  std::vector<DirectionalLight> directionalLights;
  std::vector<PointLight> pointLights;

  virtual void loadAllModels() = 0;
  virtual void addLights() = 0;
  Model& loadModel(const std::string& filepath);
  SpaceModel& loadSpaceModel(const std::string& filepath);
  Skybox& loadSkybox(const std::vector<std::string>& filepaths);

  // Give a model a name if you want to reference its transform in Application's
  // onUpdate() loop
  template <typename T, typename U>
  void addObjectToScene(T& modelRef, ShaderType type, U&& transform,
                        const std::optional<std::string> name = std::nullopt) {
    sceneObjects[type].emplace_back(
        RenderableObject{std::ref(modelRef), std::forward<U>(transform)});

    if (name.has_value())
      objectTransforms.emplace(name.value(),
                               std::ref(sceneObjects[type].back().transform));
  }

  //Give a light a name, if you'd like to access it later
  template <typename LightType>
  void addLightToScene(std::vector<LightType>& lightVec, LightType&& light,
                       const std::optional<std::string> name = std::nullopt) {
    lightVec.emplace_back(std::forward<LightType>(light));

    if (name.has_value()) 
        namedLights.emplace(name.value(), std::ref(lightVec.back()));
  }
};