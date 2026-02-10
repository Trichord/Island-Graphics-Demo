#include "scene_resources.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

#include "cube_mapped_model.h"
#include "instanced_model.h"
#include "model.h"
#include "texture_manager.h"

inline constexpr int maxModels{50};
inline constexpr int maxInstancedModels{25};
inline constexpr int maxCubeMappedModels{10};

SceneResources::SceneResources(TextureManager &_textureManager) : textureManager{_textureManager}
{
    models.reserve(maxModels);
    instancedModels.reserve(maxInstancedModels);
    cubeMappedModels.reserve(maxCubeMappedModels);
}

Model &SceneResources::loadModel(const std::filesystem::path &filepath)
{
    if (models.size() >= maxModels)
        throw std::runtime_error("Max number of models reached!");

    return models.emplace_back(textureManager, filepath);
}

Model &SceneResources::loadModel(const ModelHandle borrowHandle, const std::filesystem::path &filepath)
{
    if (models.size() >= maxModels)
        throw std::runtime_error("Max number of models reached!");

    return models.emplace_back(textureManager, borrowHandle, filepath);
}

InstancedModel &SceneResources::loadInstancedModel(const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    if (instancedModels.size() >= maxInstancedModels)
        throw std::runtime_error{"Max number of instanced models reached!"};

    return instancedModels.emplace_back(textureManager, modelPath, positionsPath, minSize, maxSize, numPositions);
}

InstancedModel &SceneResources::loadInstancedModel(const ModelHandle borrowHandle,
                                                   const std::filesystem::path &modelPath,
                                                   const std::filesystem::path &positionsPath, float minSize,
                                                   float maxSize, unsigned int numPositions)
{
    if (instancedModels.size() >= maxInstancedModels)
        throw std::runtime_error{"Max number of instanced models reached!"};

    return instancedModels.emplace_back(textureManager, borrowHandle, modelPath, positionsPath, minSize, maxSize,
                                        numPositions);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const std::filesystem::path &modelPath,
                                                     const std::vector<std::filesystem::path> &imagePaths, bool skybox)
{
    if (cubeMappedModels.size() >= maxCubeMappedModels)
        throw std::runtime_error("Max number of space models reached!");

    return cubeMappedModels.emplace_back(textureManager, modelPath, imagePaths, skybox);
}

CubeMappedModel &SceneResources::loadCubeMappedModel(const ModelHandle borrowHandle,
                                                     const std::filesystem::path &modelPath)
{
    if (cubeMappedModels.size() >= maxCubeMappedModels)
        throw std::runtime_error("Max number of space models reached!");

    return cubeMappedModels.emplace_back(textureManager, borrowHandle, modelPath);
}