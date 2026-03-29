#pragma once

#include <limits>

#include "jolt/jolt.h"

#include "jolt/Geometry/Triangle.h"
#include "scene.h"

namespace physicsUtilities
{
struct CollisionMesh
{
    JPH::TriangleList mesh;
    float minimumHeight{std::numeric_limits<float>::infinity()};
};

CollisionMesh buildCollisionMesh(Scene &scene);
} // namespace physicsUtilities