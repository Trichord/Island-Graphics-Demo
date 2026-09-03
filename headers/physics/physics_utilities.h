#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <limits>
#include <vector>

#include "scene/handle.h"
#include "scene/scene.h"

namespace physics
{
struct StaticBodies
{
    std::vector<JPH::BodyID> ids;
    float minimumHeight{std::numeric_limits<float>::infinity()};
};

struct BodyIdentifier
{
    Handle handle{0};
    std::uint32_t transformIndex{0};
};

StaticBodies buildCollisionGeometry(const Scene &scene, JPH::BodyInterface &bodyInterface);

inline JPH::uint64 createBodyIdentifier(Handle handle, std::size_t transformIndex)
{
    assert(transformIndex <= std::numeric_limits<std::uint32_t>().max());
    return static_cast<JPH::uint64>(handle) << 32 | static_cast<JPH::uint64>(transformIndex);
}

inline BodyIdentifier getBodyIdentifier(JPH::uint64 userData)
{
    JPH::uint64 handle{userData >> 32};
    JPH::uint64 transformIndex{userData & 0x00000000FFFFFFFF};
    return {.handle = static_cast<Handle>(handle), .transformIndex = static_cast<std::uint32_t>(transformIndex)};
}

} // namespace physics