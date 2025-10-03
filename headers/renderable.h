#pragma once

#include <utility>

#include "shader.h"
#include "transform.h"

class Renderable {
 public:
  virtual void render(Shader& shader) = 0;
  virtual ~Renderable() = default;
};

struct RenderableObject {
  std::reference_wrapper<Renderable> renderable;
  Transform transform;

  // RenderableObject(std::reference_wrapper<Renderable> object, Transform t) :
  // renderable(object), transform(t) {};
};