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

Model &SceneResources::loadModel(const Handle borrowHandle, const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (models_.size() >= models)
    {
        std::cerr << "Max number of models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return models_.emplace_back(textureManager_, borrowHandle, modelPath);
}

InstancedModel &SceneResources::loadInstancedModel(const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    using namespace sceneMaxObjects;

    if (instancedModels_.size() >= instancedModels)
    {
        std::cerr << "Max number of instanced models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return instancedModels_.emplace_back(textureManager_, modelPath, positionsPath, minSize, maxSize, numPositions);
}

InstancedModel &SceneResources::loadInstancedModel(const Handle borrowHandle, const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    using namespace sceneMaxObjects;

    if (instancedModels_.size() >= instancedModels)
    {
        std::cerr << "Max number of instanced models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return instancedModels_.emplace_back(textureManager_, borrowHandle, modelPath, positionsPath, minSize, maxSize,
                                         numPositions);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const std::filesystem::path &modelPath,
                                                     const std::vector<std::filesystem::path> &imagePaths, bool mipmaps)
{
    using namespace sceneMaxObjects;

    if (cubeMappedModels_.size() >= cubeMappedModels)
    {
        std::cerr << "Max number of cube mapped models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return cubeMappedModels_.emplace_back(textureManager_, modelPath, imagePaths, mipmaps);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const Handle borrowHandle, const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (cubeMappedModels_.size() >= cubeMappedModels)
    {
        std::cerr << "Max number of cube mapped models reached!" << std::endl;
        throw std::runtime_error("Scene full");
    }

    return cubeMappedModels_.emplace_back(textureManager_, borrowHandle, modelPath);
}

Skybox &SceneResources::loadSkybox(const std::vector<std::filesystem::path> &imagePaths)
{
    if (skybox_.getHandle() != 0)
        std::cerr << "WARNING: Skybox not null - overwritten";

    skybox_ = {textureManager_, imagePaths};

    return skybox_;
}

Skybox &SceneResources::loadSkybox(Handle borrowHandle)
{
    if (skybox_.getHandle() != 0)
        std::cerr << "WARNING: Skybox not null - overwritten";

    skybox_ = {textureManager_, borrowHandle};

    return skybox_;
}