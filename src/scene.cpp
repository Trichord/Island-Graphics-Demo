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

Model& Scene::loadModel(const std::string& filepath) {
  return models.emplace_back(filepath);
}

SpaceModel& Scene::loadSpaceModel(const std::string& filepath) {
  return spaceModels.emplace_back(filepath);
}

Skybox& Scene::loadSkybox(const std::vector<std::string>& filepaths) {
  return skyboxes.emplace_back(filepaths);
}

 Transform& Scene::getObjectTransform(const std::string& key) {
  return objectTransforms.at(key).get();
 }