#pragma once

#include <functional>
#include <optional>
#include <span>

#include "aabb.h"
#include "primitive.h"
#include "texture_manager.h"
#include "transform.h"
#include "instance_data.h"

class Renderable {
 public:
  virtual AABB& getAABB() = 0;
  virtual void sortInstances() = 0;
  virtual void resetVisibility() = 0;
  virtual std::size_t getInstanceCount() const = 0;
  virtual std::span<Primitive::Mesh> getMeshes() = 0;
  virtual const ModelHandle getModelHandle() const = 0;
  virtual bool perMeshCulling() const { return false; };
  virtual std::optional<std::span<InstanceData>> getInstanceData() = 0;
  virtual ~Renderable() = default;
};

struct RenderableObject {
  std::reference_wrapper<Renderable> renderable;
  Transform transform;
  bool isVisible{false};
};