#include "scene.h"

#include <map>
#include <stdexcept>
#include <vector>
#include <iostream>

#include "camera.h"
#include "directional_light.h"
#include "light.h"
#include "model.h"
#include "point_light.h"
#include "quad.h"
#include "shader_types.h"
#include "skybox.h"
#include "space_model.h"
#include "transform.h"

Scene::Scene(TextureManager& texManager) : textureManager{texManager} {
  models.reserve(sceneMaxObjects::maxModels);
  instancedModels.reserve(sceneMaxObjects::maxInstancedModels);
  spaceModels.reserve(sceneMaxObjects::maxSpaceModels);
  skyboxes.reserve(sceneMaxObjects::maxSkyboxes);
  quads.reserve(sceneMaxObjects::maxQuadModels);
  pointLights.reserve(sceneMaxObjects::maxLights);
}

std::map<ShaderType, std::vector<RenderableObject>>& Scene::getSceneObjects() {
  return sceneObjects;
}

Model& Scene::loadModel(const std::filesystem::path& filepath) {
  if (models.size() >= sceneMaxObjects::maxModels)
    throw std::runtime_error("Max number of models reached");

  return models.emplace_back(textureManager, filepath);
}

Model& Scene::loadModel(const Model& borrowTarget,
                        const std::filesystem::path& filepath) {
  if (models.size() >= sceneMaxObjects::maxModels)
    throw std::runtime_error("Max number of models reached");

  return models.emplace_back(textureManager, borrowTarget, filepath);
}

SpaceModel& Scene::loadSpaceModel(
    const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths) {
  if (spaceModels.size() >= sceneMaxObjects::maxSpaceModels)
    throw std::runtime_error("Max number of space models reached");

  return spaceModels.emplace_back(textureManager, modelPath, imagePaths);
}

SpaceModel& Scene::loadSpaceModel(
    const SpaceModel& borrowTarget, const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths) {
  if (spaceModels.size() >= sceneMaxObjects::maxSpaceModels)
    throw std::runtime_error("Max number of space models reached");

  return spaceModels.emplace_back(textureManager, borrowTarget, modelPath,
                                  imagePaths);
}

Skybox& Scene::loadSkybox(const std::vector<std::filesystem::path>& filePaths) {
  if (skyboxes.size() >= sceneMaxObjects::maxSkyboxes)
    throw std::runtime_error("Max number of skyboxes reached");

  return skyboxes.emplace_back(textureManager, filePaths);
}

Skybox& Scene::loadSkybox(const Skybox& borrowTarget,
                          const std::vector<std::filesystem::path>& filePaths) {
  if (skyboxes.size() >= sceneMaxObjects::maxSkyboxes)
    throw std::runtime_error("Max number of skyboxes reached");

  return skyboxes.emplace_back(textureManager, borrowTarget, filePaths);
}

Quad& Scene::loadQuad(const std::filesystem::path& texturePath,
                      const std::filesystem::path& positionsPath,
                      Quad::QuadPattern pattern, float minSize, float maxSize,
                      unsigned int density, unsigned int numPositions) {
  if (quads.size() >= sceneMaxObjects::maxQuadModels)
    throw std::runtime_error("Max number of quad models reached");

  return quads.emplace_back(textureManager, texturePath, positionsPath, pattern,
                            minSize, maxSize, density, numPositions);
}

Quad& Scene::loadQuad(const Quad& borrowTarget,
                      const std::filesystem::path& texturePath,
                      const std::filesystem::path& positionsPath,
                      Quad::QuadPattern pattern, float minSize, float maxSize,
                      unsigned int density, unsigned int numPositions) {
  if (quads.size() >= sceneMaxObjects::maxQuadModels)
    throw std::runtime_error("Max number of quad models reached");

  return quads.emplace_back(textureManager, borrowTarget, texturePath,
                            positionsPath, pattern, minSize, maxSize, density,
                            numPositions);
}

InstancedModel& Scene::loadInstancedModel(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& positionsPath, float minSize, float maxSize,
    unsigned int numPositions) {
  if (instancedModels.size() >= sceneMaxObjects::maxInstancedModels)
    throw std::runtime_error{"Max number of instanced models reached"};

  return instancedModels.emplace_back(textureManager, modelPath, positionsPath,
                                      minSize, maxSize, numPositions);
}

InstancedModel& Scene::loadInstancedModel(
    const InstancedModel& borrowTarget, const std::filesystem::path& modelPath,
    const std::filesystem::path& positionsPath, float minSize, float maxSize,
    unsigned int numPositions) {
  if (instancedModels.size() >= sceneMaxObjects::maxInstancedModels)
    throw std::runtime_error{"Max number of instanced models reached"};

  return instancedModels.emplace_back(textureManager, borrowTarget, modelPath,
                                      positionsPath, minSize, maxSize,
                                      numPositions);
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
  std::cout << "Loading Models...";
  addAllModelsToScene();
  std::cout << " Done." << std::endl;
  addAllLightsToScene();
  std::cout << "Loading Textures...";
  textureManager.processTextureQueue();
  std::cout << " Done." << std::endl;
}