#pragma once

#include <functional>
#include <span>

#include "aabb.h"
#include "instance_data.h"
#include "primitive.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

class IRenderable
{
  public:
    virtual AABB &getAABB() = 0;
    virtual void setVisibility(bool visible) = 0;
    virtual std::span<Primitive::Mesh> getMeshes() = 0;
    virtual const ModelHandle getHandle() const = 0;

    // Instance based methods
    virtual std::span<InstancedData> getInstanceData() = 0;

    virtual ~IRenderable() = default;
};

struct ObjectProperties
{
    Transform transform;
    CullType cullType{CullType::MODEL};
    bool isVisible{false};
    std::uint32_t visibleInstances{0};
    std::uint32_t instancedIndicesOffset{0};
};

struct Renderable
{
    std::reference_wrapper<IRenderable> object;
    ObjectProperties properties;
};