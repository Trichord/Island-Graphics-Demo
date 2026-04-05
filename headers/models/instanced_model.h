#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "glad/glad.h"
#include "model.h"
#include "renderable.h"
#include "texture_manager.h"

class InstancedModel : public Model
{
  public:
    InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                   const std::filesystem::path &positionsPath, float minSize = 1.0f, float maxSize = 1.0f,
                   unsigned int numPositions = 0);
    InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                   const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                   float minSize = 1.0f, float maxSize = 1.0f, unsigned int numPositions = 0);

    // Load positions from storage and generate matrices
    std::optional<std::vector<BoundedTransform>> generateInstancedTransforms() override;

  private:
    unsigned int numInstances_{0};
    float minSize_{0.0f};
    float maxSize_{0.0f};
    std::filesystem::path positionsPath_;

    std::vector<glm::vec3> loadPositions();
    std::vector<BoundedTransform> randomizeRotationScale(std::vector<glm::vec3> &positions);
};
