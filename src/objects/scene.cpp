#include "scene.h"
#include "camera.h"
#include "shader_types.h"
#include "model.h"
#include "space_model.h"
#include "skybox.h"
#include "transform.h"
#include "grass.h"
#include "light.h"
#include "directional_light.h"
#include "point_light.h"
#include <vector>
#include <map>
#include <stdexcept>

Scene::Scene() {
  models.reserve(sceneMaxObjects::maxModels);
  spaceModels.reserve(sceneMaxObjects::maxSpaceModels);
  skyboxes.reserve(sceneMaxObjects::maxSkyboxes);
  grasses.reserve(sceneMaxObjects::maxGrasses);
  pointLights.reserve(sceneMaxObjects::maxLights);
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

Model& Scene::loadModel(const std::filesystem::path& filepath) {
  if (models.size() >= sceneMaxObjects::maxModels)
    throw std::runtime_error("Max number of models reached");

  return models.emplace_back(filepath);
}

SpaceModel& Scene::loadSpaceModel(
    const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths) {
  if (spaceModels.size() >= sceneMaxObjects::maxSpaceModels)
    throw std::runtime_error("Max number of space models reached");

  return spaceModels.emplace_back(modelPath, imagePaths);
}

Skybox& Scene::loadSkybox(const std::vector<std::filesystem::path>& filePaths) {
  if (skyboxes.size() >= sceneMaxObjects::maxSkyboxes)
    throw std::runtime_error("Max number of skyboxes reached");

  return skyboxes.emplace_back(filePaths);
}

Grass& Scene::loadGrass(const std::filesystem::path& positionsPath,
                        const std::filesystem::path& texturePath,
                        Grass::TuftPattern tuftPattern, float minSize,
                        float maxSize, unsigned int tuftDensity,
                        unsigned int numPositions) {
  if (grasses.size() >= sceneMaxObjects::maxGrasses)
    throw std::runtime_error("Max number of grasses reached");

  return grasses.emplace_back(positionsPath, texturePath, tuftPattern, minSize, maxSize,
                              tuftDensity, numPositions);
}

Transform& Scene::getObjectTransform(const std::string& key) {
  return namedTransforms.at(key).get();
}

Light& Scene::getNamedLight(const std::string& key) {
  return namedLights.at(key).get();
}

DirectionalLight& Scene::getGlobalLightSource() { return globalLightSource; }

std::vector<PointLight>& Scene::getPointLights() { return pointLights; }

void Scene::initializeScene() {
  setupCamera();
  addAllModelsToScene();
  addAllLightsToScene();
}