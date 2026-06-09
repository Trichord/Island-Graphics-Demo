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
    std::ifstream positionsFile{positionsPath_};

    if (!positionsFile.is_open())
    {
        std::cerr << "Unable to open " << positionsPath_ << std::endl;
        return {};
    }

    std::vector<glm::vec3> instances;

    float px{0.0f};
    float py{0.0f};
    float pz{0.0f};
    float nx{0.0f};
    float ny{0.0f};
    float nz{0.0f};

    while (positionsFile >> px >> py >> pz >> nx >> ny >> nz)
    {
        instances.emplace_back(glm::vec3{px, py, pz});
        instances.emplace_back(glm::vec3{nx, ny, nz});
    }

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
    transforms.reserve(positions.size() / 2);

    for (std::size_t i{0}; i < positions.size(); i += 2)
    {
        // Rotate the model somewhere in the 360 degree range
        float instanceRotation{randomFloat(0.0f, 360.0f, mt)};
        float instanceScale{randomFloat(randomize_.minScale, randomize_.maxScale, mt)};

        Transform transform{positions[i]};

        transform.rotateAroundAxisLocal(instanceRotation, WorldVectors::worldUp);
        transform.setAbsoluteScale(instanceScale);

        glm::vec3 normal{positions[i + 1]};
        glm::vec3 axis{glm::cross(WorldVectors::worldUp, normal)};
        float surfaceAlignment{glm::clamp(glm::dot(normal, WorldVectors::worldUp), -1.0f, 1.0f)};
        float angle{glm::degrees(glm::acos(surfaceAlignment))};

        if (glm::length(axis) > 0.0001f)
            transform.rotateAroundAxisWorld(angle, axis);

        transforms.emplace_back(std::move(transform));
    }

    return transforms;
}