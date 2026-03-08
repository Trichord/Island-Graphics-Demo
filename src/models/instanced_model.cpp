#include "instanced_model.h"
#include <fstream>
#include <iostream>
#include <random>
#include <utility>

#include "aabb.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "instance_data.h"
#include "model.h"
#include "texture_manager.h"
#include <span>

InstancedModel::InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                               const std::filesystem::path &positionsPath, float minSize, float maxSize,
                               unsigned int numPositions)
    : Model{textureManager, modelPath}, numInstances_(numPositions), minSize_(minSize), maxSize_(maxSize),
      positionsPath_{positionsPath}
{
}

InstancedModel::InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                               const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                               float minSize, float maxSize, unsigned int numPositions)
    : Model{textureManager, borrowHandle, modelPath}, numInstances_(numPositions), minSize_(minSize), maxSize_(maxSize),
      positionsPath_{positionsPath}
{
}

std::optional<std::vector<InstancedData>> InstancedModel::generateInstancedData()
{
    std::vector<InstancedData> instances;

    if (!loadPositions(instances))
        return std::nullopt;
    
    generateInstanceMatrices(instances);
    generateWorldAABBs(instances);

    return instances;
}

bool InstancedModel::loadPositions(std::vector<InstancedData> &instances)
{
    instances.reserve(numInstances_);

    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    std::fstream positionsFile{positionsPath_};

    if (!positionsFile.is_open())
    {
        std::cerr << "Unable to open " << positionsPath_ << std::endl;
        return false;
    }

    while (positionsFile >> x >> y >> z)
    {
        instances.emplace_back();
        instances.back().position = glm::vec3{x, y, z};
    }

    if (instances.size() != numInstances_ && numInstances_ != 0)
        std::cout << positionsPath_ << " expected " << numInstances_ << " allocated " << instances.size() << std::endl;

    return true;
}

void InstancedModel::generateInstanceMatrices(std::vector<InstancedData> &instances)
{
    std::random_device rd;
    std::mt19937 mt(rd());

    // Rotate the model somewhere in the 360 degree range
    std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
    std::uniform_real_distribution<float> randScale{minSize_, maxSize_};

    for (auto &instance : instances)
    {
        float instanceRotation{randRotation(mt)};
        float instanceScale{randScale(mt)};

        glm::mat4 modelMatrix{1.0f};
        modelMatrix = glm::translate(modelMatrix, instance.position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(instanceRotation), glm::vec3{0.0f, 1.0f, 0.0f});
        modelMatrix = glm::scale(modelMatrix, glm::vec3{instanceScale});

        instance.modelMatrix = modelMatrix;
    }
}

void InstancedModel::generateWorldAABBs(std::vector<InstancedData> &instances)
{
    for (auto &instance : instances)
    {
        glm::mat3 RS{glm::mat3{instance.modelMatrix}};
        glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

        glm::vec3 worldCenter{instance.modelMatrix * glm::vec4(aabb_.getCenter(), 1.0f)};
        glm::vec3 worldExtent{absRS * aabb_.getExtent()};

        instance.worldAABB = AABB{worldCenter - worldExtent, worldCenter + worldExtent};
    }
}
