#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "models/model.h"
#include "utilities/transform.h"

class InstancedModel : public Model
{
  public:
    struct Randomize
    {
        float minScale{1.0f};
        float maxScale{1.0f};
    };

    InstancedModel(const std::filesystem::path &modelPath, ShaderType shader,
                   const std::filesystem::path &positionsPath, Randomize randomize);

    // Load positions from storage and generate matrices
    std::vector<Transform> loadInstancedTransforms() const;

  private:
    std::filesystem::path positionsPath_;
    Randomize randomize_;

    std::vector<glm::vec3> loadPositions() const;
    std::vector<Transform> randomizeRotationScale(std::vector<glm::vec3> &positions) const;
};
