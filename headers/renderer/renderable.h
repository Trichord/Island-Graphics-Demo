#pragma once

#include <array>
#include <functional>

#include "glm/glm.hpp"
#include "transform.h"

class Renderable {
 public:
  virtual void render() = 0;
  virtual bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                              const glm::mat4& modelMatrix) = 0;
  virtual ~Renderable() = default;
};

struct RenderableObject {
  std::reference_wrapper<Renderable> renderable;
  Transform transform;
  bool isVisible{false};
};