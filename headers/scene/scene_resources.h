#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "texture_manager.h"

class SceneResources
{
  public:
    SceneResources(TextureManager &textureManager);

    Model &loadModel(const std::filesystem::path &filepath);
    Model &loadModel(const Model &borrowTarget, const std::filesystem::path &filepath);

    InstancedModel &loadInstancedModel(const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);
    InstancedModel &loadInstancedModel(const InstancedModel &borrowTarget, const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);

    CubeMappedModel &loadCubeMappedModel(const std::filesystem::path &modelPath,
                                         const std::vector<std::filesystem::path> &imagePaths, bool skybox = false);
    CubeMappedModel &loadCubeMappedModel(const CubeMappedModel &borrowTarget, const std::filesystem::path &modelPath);

    void loadingComplete();

  private:
    TextureManager &textureManager;
    std::vector<Model> models;
    std::vector<CubeMappedModel> cubeMappedModels;
    std::vector<InstancedModel> instancedModels;
};