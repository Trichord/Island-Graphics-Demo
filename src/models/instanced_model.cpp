#include "instanced_model.h"

#include <fstream>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "model.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "texture_manager.h"
#include "transform.h"
#include "world_vectors.h"

InstancedModel::InstancedModel(const std::filesystem::path &modelPath,
                               const std::filesystem::path &positionsPath, Randomize randomize)
    : Model{modelPath}, positionsPath_{positionsPath}, randomize_{randomize}
{
}

std::vector<Transform> InstancedModel::loadInstancedTransforms() const
{
    std::vector<glm::vec3> positions{loadPositions()};

    if (positions.empty())
        return {};

    std::vector<Transform> transforms{randomizeRotationScale(positions)};

    return transforms;
}

std::vector<glm::vec3> InstancedModel::loadPositions() const
{
    std::vector<glm::vec3> instances;

    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    std::ifstream positionsFile{positionsPath_};

    if (!positionsFile.is_open())
    {
        std::cerr << "Unable to open " << positionsPath_ << std::endl;
        return {};
    }

    while (positionsFile >> x >> y >> z)
        instances.emplace_back(glm::vec3{x, y, z});

    return instances;
}

std::vector<Transform> InstancedModel::randomizeRotationScale(std::vector<glm::vec3> &positions) const
{
    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<Transform> transforms;
    transforms.reserve(positions.size());

    // Rotate the model somewhere in the 360 degree range
    std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
    std::uniform_real_distribution<float> randScale{randomize_.minScale, randomize_.maxScale};

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