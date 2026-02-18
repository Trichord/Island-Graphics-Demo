#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "texture_manager.h"


namespace sceneMaxObjects
{
inline constexpr int maxModels{50};
inline constexpr int maxInstancedModels{25};
inline constexpr int maxCubeMappedModels{10};
inline constexpr int totalMaxModels{maxModels + maxInstancedModels + maxCubeMappedModels};
inline constexpr int maxLights{5};
} // namespace sceneMaxObjects

class SceneResources
{
  public:
    SceneResources(TextureManager &textureManager);

    Model &loadModel(const std::filesystem::path &filepath);
    Model &loadModel(const ModelHandle borrowHandle, const std::filesystem::path &filepath);

    InstancedModel &loadInstancedModel(const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);
    InstancedModel &loadInstancedModel(const ModelHandle borrowHandle, const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);

    CubeMappedModel &loadCubeMappedModel(const std::filesystem::path &modelPath,
                                         const std::vector<std::filesystem::path> &imagePaths, bool skybox = false);
    CubeMappedModel &loadCubeMappedModel(const ModelHandle borrowHandle, const std::filesystem::path &modelPath);

  private:
    TextureManager &textureManager;
    std::vector<Model> models;
    std::vector<CubeMappedModel> cubeMappedModels;
    std::vector<InstancedModel> instancedModels;
};