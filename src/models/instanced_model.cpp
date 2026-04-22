#include "instanced_model.h"

#include <fstream>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "base_model.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "texture_manager.h"
#include "transform.h"
#include "world_vectors.h"

InstancedModel::InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                               const std::filesystem::path &positionsPath, float minSize, float maxSize,
                               int numPositions)
    : BaseModel{textureManager, modelPath}, numInstances_(numPositions), minSize_(minSize), maxSize_(maxSize),
      positionsPath_{positionsPath}
{
}

InstancedModel::InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                               const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                               float minSize, float maxSize, int numPositions)
    : BaseModel{textureManager, borrowHandle, modelPath}, numInstances_(numPositions), minSize_(minSize),
      maxSize_(maxSize), positionsPath_{positionsPath}
{
}

std::vector<Transform> InstancedModel::loadInstancedTransforms()
{
    std::vector<glm::vec3> positions{loadPositions()};

    if (positions.empty())
        return {};

    std::vector<Transform> transforms{randomizeRotationScale(positions)};

    return transforms;
}

std::vector<glm::vec3> InstancedModel::loadPositions()
{
    std::vector<glm::vec3> instances;
    instances.reserve(numInstances_);

    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    std::fstream positionsFile{positionsPath_};

    if (!positionsFile.is_open())
    {
        std::cerr << "Unable to open " << positionsPath_ << std::endl;
        return {};
    }

    while (positionsFile >> x >> y >> z)
        instances.emplace_back(glm::vec3{x, y, z});

    if (instances.size() != numInstances_ && numInstances_ != 0)
        std::cout << positionsPath_ << " expected " << numInstances_ << " allocated " << instances.size() << std::endl;

    return instances;
}

std::vector<Transform> InstancedModel::randomizeRotationScale(std::vector<glm::vec3> &positions)
{
    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<Transform> transforms;
    transforms.reserve(positions.size());

    // Rotate the model somewhere in the 360 degree range
    std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
    std::uniform_real_distribution<float> randScale{minSize_, maxSize_};

    for (auto &position : positions)
    {
        float instanceRotation{randRotation(mt)};
        float instanceScale{randScale(mt)};

        Transform transform{position};

        transform.rotateAroundAxis(instanceRotation, WorldVectors::worldUp);
        transform.setAbsoluteScale(instanceScale);

        transforms.emplace_back(std::move(transform));
    }

    return transforms;
}