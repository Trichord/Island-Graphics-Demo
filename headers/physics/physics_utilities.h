#pragma once

#include <jolt/Jolt.h>

#include <jolt/Geometry/Triangle.h>
#include <limits>

#include "scene/scene.h"

namespace physicsUtilities
{
struct CollisionMesh
{
    JPH::TriangleList mesh;
    float minimumHeight{std::numeric_limits<float>::infinity()};
};

CollisionMesh buildCollisionMesh(Scene &scene);
} // namespace physicsUtilities