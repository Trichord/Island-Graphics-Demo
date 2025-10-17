#include "../headers/scene.h"
#include "../headers/camera.h"
#include "../headers/shader_types.h"
#include "../headers/model.h"
#include "../headers/space_model.h"
#include "../headers/skybox.h"
#include "../headers/transform.h"
#include "../headers/grass.h"
#include "../headers/light.h"
#include "../headers/directional_light.h"
#include "../headers/point_light.h"
#include <vector>
#include <map>
#include <stdexcept>

Scene::Scene() {
  models.reserve(MAX_MODELS);
  spaceModels.reserve(MAX_SPACE_MODELS);
  skyboxes.reserve(MAX_SKYBOXES);
  grasses.reserve(MAX_GRASSES);
  directionalLights.reserve(MAX_DIRECTIONAL_LIGHTS);
  pointLights.reserve(MAX_POINT_LIGHTS);
}

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
  if (models.size() == MAX_MODELS) throw std::runtime_error("Max number of models reached");

  return models.emplace_back(filepath);
}

SpaceModel& Scene::loadSpaceModel(const std::string& filepath) {
  if (spaceModels.size() == MAX_SPACE_MODELS)
    throw std::runtime_error("Max number of space models reached");

  return spaceModels.emplace_back(filepath);
}

Skybox& Scene::loadSkybox(const std::vector<std::string>& filepaths) {
  if (skyboxes.size() == MAX_SKYBOXES)
    throw std::runtime_error("Max number of skyboxes reached");

  return skyboxes.emplace_back(filepaths);
}

Grass& Scene::loadGrass(const std::string& filepath) {
  if (grasses.size() == MAX_GRASSES)
    throw std::runtime_error("Max number of grasses reached");

  return grasses.emplace_back(filepath);
}

Transform& Scene::getObjectTransform(const std::string& key) {
  return namedTransforms.at(key).get();
}

Light& Scene::getNamedLight(const std::string& key) {
  return namedLights.at(key).get();
}

std::vector<DirectionalLight>& Scene::getDirectionalLights() {
  return directionalLights;
}

std::vector<PointLight>& Scene::getPointLights() { return pointLights; }
