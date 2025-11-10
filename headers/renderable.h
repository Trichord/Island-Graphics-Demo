#pragma once

#include "transform.h"

class Renderable {
 public:
  virtual void render() = 0;
  virtual ~Renderable() = default;
};

struct RenderableObject {
  std::reference_wrapper<Renderable> renderable;
  Transform transform;
};