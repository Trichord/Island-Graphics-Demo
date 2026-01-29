#pragma once

#include <functional>
#include <span>

#include "aabb.h"
#include "instance_data.h"
#include "primitive.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

class Renderable {
 public:
  virtual AABB& getAABB() = 0;
  virtual void resetVisibility() = 0;
  virtual std::size_t getInstanceCount() const = 0;
  virtual std::span<Primitive::Mesh> getMeshes() = 0;
  virtual const ModelHandle getModelHandle() const = 0;
  virtual bool perMeshCulling() const { return false; };
  virtual std::span<InstanceData> getInstanceData() = 0;
  virtual void setVisibleInstanceCount(std::size_t newSize) = 0;
  virtual ~Renderable() = default;
};

struct RenderableData {
  std::reference_wrapper<Renderable> object;
  Transform transform;
  CullStrategy cullStrategy{CullStrategy::MODEL};
  bool isVisible{true};
};