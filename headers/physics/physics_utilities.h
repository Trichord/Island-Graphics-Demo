#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <limits>
#include <vector>

#include "scene/scene.h"

namespace physics
{
struct StaticBodies
{
    std::vector<JPH::BodyID> ids;
    float minimumHeight{std::numeric_limits<float>::infinity()};
};

StaticBodies buildCollisionGeometry(const Scene &scene, JPH::BodyInterface &bodyInterface);
} // namespace physics