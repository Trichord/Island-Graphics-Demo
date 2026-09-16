#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "models/model.h"
#include "renderer/shader/graphics_shader.h"
#include "utilities/transform.h"

class InstancedModel : public Model
{
  public:
    InstancedModel(const std::filesystem::path &modelPath, ShaderType shader,
                   const std::filesystem::path &positionsPath, float minScale = 1.0f, float maxScale = 1.0f);

    // Load positions from storage and generate matrices
    std::vector<Transform> loadInstancedTransforms() const;

  private:
    std::filesystem::path positionsPath_;
    float minScale_{1.0f};
    float maxScale_{1.0f};

    std::vector<glm::vec3> loadPositions() const;
    std::vector<Transform> randomizeRotationScale(std::vector<glm::vec3> &positions) const;
};
