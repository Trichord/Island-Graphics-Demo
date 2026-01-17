#pragma once

#include <array>

#include "glm/glm.hpp"
#include "model.h"

class Terrain : public Model {
  using Model::Model;

  bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                      const glm::mat4& modelMatrix, const glm::vec3& cameraPos,
                      const float cullDistanceScale) override;
};