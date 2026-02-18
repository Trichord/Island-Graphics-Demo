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
    : Model{textureManager, modelPath}
{
    instances.reserve(numPositions);
    loadPositions(positionsPath, numPositions);
    generateInstanceMatrices(minSize, maxSize);
    generateWorldAABBs();
}

InstancedModel::InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                               const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                               float minSize, float maxSize, unsigned int numPositions)
    : Model{textureManager, borrowHandle, modelPath}
{
    loadPositions(positionsPath, numPositions);
    generateInstanceMatrices(minSize, maxSize);
    generateWorldAABBs();
}

InstancedModel::InstancedModel(InstancedModel &&other) noexcept : Model{std::move(other)}
{
    instances = std::move(other.instances);
    instancesVBO = other.instancesVBO;

    other.instancesVBO = 0;
}

InstancedModel::~InstancedModel()
{
    deleteBuffers();
}

void InstancedModel::loadPositions(const std::filesystem::path &positionsPath, unsigned int numPositions)
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    std::fstream positionsFile{positionsPath};

    if (!positionsFile.is_open())
    {
        std::cerr << "Unable to open " << positionsPath << std::endl;
        return;
    }

    while (positionsFile >> x >> y >> z)
    {
        instances.emplace_back();
        instances.back().position = glm::vec3{x, y, z};
    }

    if (instances.size() != numPositions && numPositions != 0)
        std::cout << positionsPath << " expected " << numPositions << " allocated " << instances.size() << std::endl;
}

void InstancedModel::generateInstanceMatrices(float minSize, float maxSize)
{
    std::random_device rd;
    std::mt19937 mt(rd());

    // Rotate the model somewhere in the 360 degree range
    std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
    std::uniform_real_distribution<float> randScale{minSize, maxSize};

    for (auto &instance : instances)
    {
        float instanceRotation{randRotation(mt)};
        float instanceScale{randScale(mt)};

        glm::mat4 modelMatrix{1.0f};
        modelMatrix = glm::translate(modelMatrix, instance.position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(instanceRotation), glm::vec3{0.0f, 1.0f, 0.0f});
        modelMatrix = glm::scale(modelMatrix, glm::vec3{instanceScale});

        glm::mat4 normalMatrix{glm::transpose(glm::inverse(glm::mat3{modelMatrix}))};

        instance.modelMatrix = modelMatrix;
    }
}

void InstancedModel::generateWorldAABBs()
{
    for (auto &instance : instances)
    {
        glm::mat3 RS{glm::mat3{instance.modelMatrix}};
        glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

        glm::vec3 worldCenter{instance.modelMatrix * glm::vec4(aabb.getCenter(), 1.0f)};
        glm::vec3 worldExtent{absRS * aabb.getExtent()};

        instance.worldAABB = AABB{worldCenter - worldExtent, worldCenter + worldExtent};
    }
}

void InstancedModel::deleteBuffers()
{
    glDeleteBuffers(1, &instancesVBO);
}

std::span<InstancedData> InstancedModel::getInstanceData()
{
    return instances;
}