#include "scene_resources.h"

#include <filesystem>
#include <span>
#include <stdexcept>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "texture_manager.h"

SceneResources::SceneResources(TextureManager &textureManager) : textureManager_{textureManager}
{
    using namespace sceneMaxObjects;
    models_.reserve(maxModels);
    instancedModels_.reserve(maxInstancedModels);
    cubeMappedModels_.reserve(maxCubeMappedModels);
}

Model &SceneResources::loadModel(const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (models_.size() >= maxModels)
        throw std::runtime_error("Max number of models reached!");

    return models_.emplace_back(textureManager_, modelPath);
}

Model &SceneResources::loadModel(const ModelHandle borrowHandle, const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (models_.size() >= maxModels)
        throw std::runtime_error("Max number of models reached!");

    return models_.emplace_back(textureManager_, borrowHandle, modelPath);
}

InstancedModel &SceneResources::loadInstancedModel(const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    using namespace sceneMaxObjects;

    if (instancedModels_.size() >= maxInstancedModels)
        throw std::runtime_error{"Max number of instanced models reached!"};

    return instancedModels_.emplace_back(textureManager_, modelPath, positionsPath, minSize, maxSize, numPositions);
}

InstancedModel &SceneResources::loadInstancedModel(const ModelHandle borrowHandle,
                                                   const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    using namespace sceneMaxObjects;

    if (instancedModels_.size() >= maxInstancedModels)
        throw std::runtime_error{"Max number of instanced models reached!"};

    return instancedModels_.emplace_back(textureManager_, borrowHandle, modelPath, positionsPath, minSize, maxSize,
                                         numPositions);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const std::filesystem::path &modelPath,
                                                     const std::vector<std::filesystem::path> &imagePaths, bool skybox)
{
    using namespace sceneMaxObjects;

    if (cubeMappedModels_.size() >= maxCubeMappedModels)
        throw std::runtime_error("Max number of space models reached!");

    return cubeMappedModels_.emplace_back(textureManager_, modelPath, imagePaths, skybox);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const ModelHandle borrowHandle,
                                                     const std::filesystem::path &modelPath)
{
    using namespace sceneMaxObjects;

    if (cubeMappedModels_.size() >= maxCubeMappedModels)
        throw std::runtime_error("Max number of space models reached!");

    return cubeMappedModels_.emplace_back(textureManager_, borrowHandle, modelPath);
}