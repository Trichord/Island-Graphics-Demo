#pragma once

#include <functional>
#include <span>

#include "aabb.h"
#include "instance_data.h"
#include "primitive.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

class Renderable
{
  public:
    virtual AABB &getAABB() = 0;
    virtual void resetVisibility() = 0;
    virtual std::span<Primitive::Mesh> getMeshes() = 0;
    virtual const ModelHandle getMaterialID() const = 0;
    virtual const ModelHandle getModelHandle() const = 0;
    virtual bool perMeshCulling() const
    {
        return false;
    };

    // Instance based methods
    virtual std::size_t getInstanceCount() const = 0;
    virtual std::span<InstanceData> getInstanceData() = 0;
    virtual void setVisibleInstanceCount(std::size_t newSize) = 0;

    virtual ~Renderable() = default;
};

struct ObjectProperties
{
    Transform transform;
    CullType cullType{CullType::MODEL};
    bool isVisible{true};
    bool cullByMesh{false};

    bool isInstanced() const
    {
        using enum CullType;
        return cullType == INSTANCED || cullType == INSTANCED_DISTANCE;
    }
};

struct RenderableInstance
{
    std::reference_wrapper<Renderable> object;
    ObjectProperties properties;
};