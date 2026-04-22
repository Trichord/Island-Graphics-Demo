#pragma once

#include <filesystem>
#include <vector>

#include "base_model.h"
#include "glm/glm.hpp"
#include "texture_manager.h"
#include "transform.h"

class InstancedModel : public BaseModel
{
  public:
    InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                   const std::filesystem::path &positionsPath, float minSize = 1.0f, float maxSize = 1.0f,
                   int numPositions = 0);
    InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                   const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                   float minSize = 1.0f, float maxSize = 1.0f, int numPositions = 0);

    // Load positions from storage and generate matrices
    std::vector<Transform> loadInstancedTransforms();

  private:
    int numInstances_{0};
    float minSize_{0.0f};
    float maxSize_{0.0f};
    std::filesystem::path positionsPath_;

    std::vector<glm::vec3> loadPositions();
    std::vector<Transform> randomizeRotationScale(std::vector<glm::vec3> &positions);
};
