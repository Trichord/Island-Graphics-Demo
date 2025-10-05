#include "../headers/scene.h"

Scene::Scene(Scene&& other) noexcept {
  sceneObjects = std::move(other.sceneObjects);
  other.sceneObjects.clear();
  camera = std::move(other.camera);
}

Scene& Scene::operator=(Scene&& other) noexcept {
  if (this != &other) {
    sceneObjects.clear();
    sceneObjects = std::move(other.sceneObjects);
    camera = std::move(other.camera);
  }

  return *this;
}

std::map<ShaderType, std::vector<RenderableObject>>& Scene::getSceneObjects() {
  return sceneObjects;
}
