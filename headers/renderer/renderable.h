#pragma once

#include <array>
#include <functional>

#include "glm/glm.hpp"
#include "gl_state.h"
#include "transform.h"

class Renderable {
 public:
  virtual void render(GLState& glState) = 0;
  virtual bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                              const glm::mat4& modelMatrix, const glm::vec3& cameraPos, const float cullDistanceScale) = 0;
  virtual ~Renderable() = default;
};

struct RenderableObject {
  std::reference_wrapper<Renderable> renderable;
  Transform transform;
  bool isVisible{false};
};