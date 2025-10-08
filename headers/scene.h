#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <string>
#include <optional>
#include <utility>

#include "model.h"
#include "space_model.h"
#include "skybox.h"
#include "camera.h"
#include "renderable.h"
#include "shader_types.h"
#include "transform.h"

//-----------------------------------------------------
// It is important for concrete subclasses to store the models in a class member
// variable. RenderableObject holds a non-owning reference to the model and it's
// critical to keep the reference alive after any loading functions finish
//
// Any container storing Model instances must guarantee that their
// addresses remain valid for the lifetime of all RenderableObjects.
//
// If using std::vector<Model>, you MUST call reserve()
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

  struct LightSources {
    glm::vec3 globalLightDirection;
    glm::vec3 pointLightPosition;
  } lightSources;

  std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects();
  Transform& getObjectTransform(const std::string& key);
  Camera camera;

 protected:
  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  std::map<std::string, std::reference_wrapper<Transform>> objectTransforms;

  std::vector<Model> models;
  std::vector<SpaceModel> spaceModels;
  std::vector<Skybox> skyboxes;

  virtual void buildScene() = 0;
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
};