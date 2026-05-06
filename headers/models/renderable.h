#pragma once

#include <optional>
#include <span>
#include <vector>

#include "aabb.h"
#include "primitive.h"
#include "texture_manager.h"

class Renderable
{
  public:   
    virtual AABB &getAABB() = 0;
    virtual Handle getHandle() const = 0;
    virtual std::span<Primitive> getPrimitives() = 0;
    virtual std::optional<Handle> getBorrowedHandle() const = 0;
    virtual std::optional<std::vector<Primitive>> loadCollisionMesh() const = 0;
    
    virtual ~Renderable() = default;
};
