#include "scene_resources.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "terrain.h"
#include "texture_manager.h"

inline constexpr int maxModels{50};
inline constexpr int maxInstancedModels{25};
inline constexpr int maxCubeMappedModels{10};
inline constexpr int maxTerrains{5};

SceneResources::SceneResources(TextureManager& _textureManager)
    : textureManager{_textureManager} {
  models.reserve(maxModels);
  instancedModels.reserve(maxInstancedModels);
  cubeMappedModels.reserve(maxCubeMappedModels);
  terrains.reserve(maxTerrains);
}

TextureManager& SceneResources::getTextureManager() { return textureManager; }

Model& SceneResources::loadModel(const std::filesystem::path& filepath) {
  if (models.size() >= maxModels)
    throw std::runtime_error("Max number of models reached!");

  return models.emplace_back(textureManager, filepath);
}

Model& SceneResources::loadModel(const Model& borrowTarget,
                                 const std::filesystem::path& filepath) {
  if (models.size() >= maxModels)
    throw std::runtime_error("Max number of models reached!");

  return models.emplace_back(textureManager, borrowTarget, filepath);
}

InstancedModel& SceneResources::loadInstancedModel(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& positionsPath, float minSize, float maxSize,
    unsigned int numPositions) {
  if (instancedModels.size() >= maxInstancedModels)
    throw std::runtime_error{"Max number of instanced models reached!"};

  return instancedModels.emplace_back(textureManager, modelPath, positionsPath,
                                      minSize, maxSize, numPositions);
}

InstancedModel& SceneResources::loadInstancedModel(
    const InstancedModel& borrowTarget, const std::filesystem::path& modelPath,
    const std::filesystem::path& positionsPath, float minSize, float maxSize,
    unsigned int numPositions) {
  if (instancedModels.size() >= maxInstancedModels)
    throw std::runtime_error{"Max number of instanced models reached!"};

  return instancedModels.emplace_back(textureManager, borrowTarget, modelPath,
                                      positionsPath, minSize, maxSize,
                                      numPositions);
}

CubeMappedModel& SceneResources::loadCubeMappedModel(
    const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths) {
  if (cubeMappedModels.size() >= maxCubeMappedModels)
    throw std::runtime_error("Max number of space models reached!");

  return cubeMappedModels.emplace_back(textureManager, modelPath, imagePaths);
}

CubeMappedModel& SceneResources::loadCubeMappedModel(
    const CubeMappedModel& borrowTarget,
    const std::filesystem::path& modelPath) {
  if (cubeMappedModels.size() >= maxCubeMappedModels)
    throw std::runtime_error("Max number of space models reached!");

  return cubeMappedModels.emplace_back(textureManager, borrowTarget, modelPath);
}

Terrain& SceneResources::loadTerrain(const std::filesystem::path& modelPath) {
  if (terrains.size() >= maxTerrains)
    throw std::runtime_error("Max number of terrains reached!");

  return terrains.emplace_back(textureManager, modelPath);
}

Terrain& SceneResources::loadTerrain(const Model& borrowTarget,
                                     const std::filesystem::path& modelPath) {
  if (terrains.size() >= maxTerrains)
    throw std::runtime_error("Max number of terrains reached!");

  return terrains.emplace_back(textureManager, borrowTarget, modelPath);
}