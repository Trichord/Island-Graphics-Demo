#include "models/instanced_model.h"

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "models/model.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

InstancedModel::InstancedModel(const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                               Randomize randomize)
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
    auto randomFloat = [](float min, float max, std::mt19937 &mt) {
        if (min == max)
            return min;

        if (max < min)
            std::swap(max, min);

        std::uniform_real_distribution<float> rnd{min, max};
        return rnd(mt);
    };

    std::random_device rd;
    std::mt19937 mt(rd());

    std::vector<Transform> transforms;
    transforms.reserve(positions.size());

    for (auto &position : positions)
    {
        // Rotate the model somewhere in the 360 degree range
        float instanceRotation{randomFloat(0.0, 360.0f, mt)};
        float instanceScale{randomFloat(randomize_.minScale, randomize_.maxScale, mt)};

        Transform transform{position};

        transform.rotateAroundAxis(instanceRotation, WorldVectors::worldUp);
        transform.setAbsoluteScale(instanceScale);

        transforms.emplace_back(std::move(transform));
    }

    return transforms;
}