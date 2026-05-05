#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "base_model.h"
#include "texture_manager.h"

namespace sceneMaxObjects
{
inline constexpr int models{50};
inline constexpr int instancedModels{25};
inline constexpr int cubeMappedModels{10};
inline constexpr int maxTotal{models + instancedModels + cubeMappedModels};
} // namespace sceneMaxObjects

class SceneResources
{
  public:
    SceneResources(TextureManager &textureManager);

    Model &loadModel(const std::filesystem::path &modelPath);
    Model &loadModel(const Handle borrowHandle, const std::filesystem::path &modelPath);

    InstancedModel &loadInstancedModel(const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);
    InstancedModel &loadInstancedModel(const Handle borrowHandle, const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath, float minSize = 1.0f,
                                       float maxSize = 1.0f, unsigned int numPositions = 0);

    CubeMappedModel &loadCubeMappedModel(const std::filesystem::path &modelPath,
                                         const std::vector<std::filesystem::path> &imagePaths, bool mipmaps = true);
    CubeMappedModel &loadCubeMappedModel(const Handle borrowHandle, const std::filesystem::path &modelPath);

  private:
    TextureManager &textureManager_;
    std::vector<Model> models_;
    std::vector<CubeMappedModel> cubeMappedModels_;
    std::vector<InstancedModel> instancedModels_;
};