#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <vector>

#include "camera.h"
#include "renderable.h"
#include "shader_types.h"

//-----------------------------------------------------
// It is important for any concrete subclass to store the models in a class
// member variable. RenderableObject holds a non-owning reference to the model
// and it's critical to keep the reference alive after any loading functions
// finish.
//
// Any container storing Model instances must guarantee that their
// addresses remain valid for the lifetime of all RenderableObjects.
//
// In concrete subclasses, store models in a class member container with stable
// element addresses. If using std::vector<Model>, you MUST call reserve()
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

 protected:
  Camera camera;
  std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
  virtual void loadModels() = 0;
};