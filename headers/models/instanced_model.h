#pragma once

#include <filesystem>
#include <vector>

#include "glm/glm.hpp"
#include "model.h"
#include "texture_manager.h"
#include "transform.h"

class InstancedModel : public Model
{
  public:
    struct Randomize
    {
        float minScale{1.0f};
        float maxScale{1.0f};
    };

    InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                   const std::filesystem::path &positionsPath, Randomize randomize);

    // Load positions from storage and generate matrices
    std::vector<Transform> loadInstancedTransforms();

  private:
    std::filesystem::path positionsPath_;
    Randomize randomize_;

    std::vector<glm::vec3> loadPositions();
    std::vector<Transform> randomizeRotationScale(std::vector<glm::vec3> &positions);
};
