#pragma once

#include <functional>
#include <span>

#include "aabb.h"
#include "cull_method.h"
#include "instance_data.h"
#include "primitive.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "transform.h"

class IRenderable
{
  public:
    virtual AABB &getAABB() = 0;
    virtual void setVisibility(bool visible) = 0;
    virtual std::span<Primitive::Mesh> getMeshes() = 0;
    virtual const ModelHandle getHandle() const = 0;
    virtual std::optional<std::vector<InstancedData>> generateInstancedData() = 0;

    virtual ~IRenderable() = default;
};

struct ObjectProperties
{
    using modelTransform = std::variant<Transform, std::vector<InstancedData>>;

    modelTransform transform{Transform{}};
    CullMethod cullMethod{CullMethod::MODEL};
    bool isVisible{false};
    std::uint32_t visibleInstances{0};
    std::uint32_t instancedIndicesOffset{0};

    bool isInstanced()
    {
        return std::holds_alternative<std::vector<InstancedData>>(transform);
    }
};

struct Renderable
{
    std::reference_wrapper<IRenderable> objectRef;
    ObjectProperties properties;
};