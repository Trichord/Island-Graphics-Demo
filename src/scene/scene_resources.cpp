#include "scene_resources.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "skybox.h"
#include "texture_manager.h"

SceneResources::SceneResources(TextureManager &textureManager) : textureManager_{textureManager}
{
    using namespace sceneMaxObjects;
    models_.reserve(models);
    instancedModels_.reserve(instancedModels);
    cubeMappedModels_.reserve(cubeMappedModels);
}

Model &SceneResources::loadModel(const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (models_.size() >= models)
    {
        std::cerr << "Max number of models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return models_.emplace_back(textureManager_, modelPath);
}

InstancedModel &SceneResources::loadInstancedModel(const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath,
                                                   const InstancedModel::Randomize randomize)
{
    using namespace sceneMaxObjects;

    if (instancedModels_.size() >= instancedModels)
    {
        std::cerr << "Max number of instanced models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return instancedModels_.emplace_back(textureManager_, modelPath, positionsPath, randomize);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const std::filesystem::path &modelPath,
                                                     const std::filesystem::path &imagePath)
{
    using namespace sceneMaxObjects;

    if (cubeMappedModels_.size() >= cubeMappedModels)
    {
        std::cerr << "Max number of cube mapped models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return cubeMappedModels_.emplace_back(textureManager_, modelPath, imagePath);
}

Skybox &SceneResources::loadSkybox(const std::filesystem::path &imagePath)
{
    if (skybox_.getHandle() != 0)
        std::cerr << "WARNING: Skybox not null - overwritten";

    skybox_ = {textureManager_, imagePath};

    return skybox_;
}