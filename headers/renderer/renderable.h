#pragma once

#include <functional>
#include <span>

#include "aabb.h"
#include "instance_data.h"
#include "primitive.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

class RenderableObject {
 public:
  virtual AABB& getAABB() = 0;
  virtual void sortInstances() = 0;
  virtual void resetVisibility() = 0;
  virtual std::size_t getInstanceCount() const = 0;
  virtual std::span<Primitive::Mesh> getMeshes() = 0;
  virtual const ModelHandle getModelHandle() const = 0;
  virtual bool perMeshCulling() const { return false; };
  virtual std::span<InstanceData> getInstanceData() = 0;
  virtual ~RenderableObject() = default;
};

struct Renderable {
  std::reference_wrapper<RenderableObject> object;
  Transform transform;
  CullStrategy cullStrategy{CullStrategy::MODEL};
  bool isVisible{true};
};