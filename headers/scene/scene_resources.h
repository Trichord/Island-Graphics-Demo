#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "skybox.h"
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

    Model &loadModel(const std::filesystem::path &modelPath, const std::optional<Handle> borrowHandle = {});

    InstancedModel &loadInstancedModel(const std::filesystem::path &modelPath,
                                       const std::filesystem::path &positionsPath,
                                       const InstancedModel::Randomize randomize,
                                       const std::optional<Handle> borrowHandle = {});

    CubeMappedModel &loadCubeMappedModel(const std::filesystem::path &modelPath,
                                         const std::filesystem::path &imagePaths,
                                         const std::optional<Handle> borrowHandle = {});

    Skybox &loadSkybox(const std::filesystem::path &imagePaths);
    Skybox &loadSkybox(const Handle borrowHandle);

  private:
    TextureManager &textureManager_;

    Skybox skybox_;
    std::vector<Model> models_;
    std::vector<InstancedModel> instancedModels_;
    std::vector<CubeMappedModel> cubeMappedModels_;
};