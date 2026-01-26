#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "skybox.h"
#include "terrain.h"
#include "texture_manager.h"

class SceneResources {
 public:
  SceneResources(TextureManager& textureManager);

  Model& loadModel(const std::filesystem::path& filepath);
  Model& loadModel(const Model& borrowTarget,
                   const std::filesystem::path& filepath);

  InstancedModel& loadInstancedModel(const std::filesystem::path& modelPath,
                                     const std::filesystem::path& positionsPath,
                                     float minSize = 1.0f, float maxSize = 1.0f,
                                     unsigned int numPositions = 0);
  InstancedModel& loadInstancedModel(const InstancedModel& borrowTarget,
                                     const std::filesystem::path& modelPath,
                                     const std::filesystem::path& positionsPath,
                                     float minSize = 1.0f, float maxSize = 1.0f,
                                     unsigned int numPositions = 0);

  CubeMappedModel& loadCubeMappedModel(
      const std::filesystem::path& modelPath,
      const std::vector<std::filesystem::path>& imagePaths);
  CubeMappedModel& loadCubeMappedModel(const CubeMappedModel& borrowTarget,
                                       const std::filesystem::path& modelPath);

  Terrain& loadTerrain(const std::filesystem::path& modelPath);
  Terrain& loadTerrain(const Model& borrowTarget,
                       const std::filesystem::path& modelPath);

  void loadSkybox(const std::vector<std::filesystem::path>& filePaths);
  void loadSkybox(const Skybox& borrowTarget);

  TextureManager& getTextureManager();
  const Skybox* getSkybox() const;

 private:
  TextureManager& textureManager;
  std::vector<Model> models;
  std::vector<CubeMappedModel> cubeMappedModels;
  std::vector<InstancedModel> instancedModels;
  std::vector<Terrain> terrains;
  std::unique_ptr<Skybox> skybox;
};