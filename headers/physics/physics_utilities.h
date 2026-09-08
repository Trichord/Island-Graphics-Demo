#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <bit>
#include <cassert>
#include <cstdint>
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

struct ObjectCollision
{
    Handle handle{0};
    float speed{0.0f};
};

StaticBodies buildCollisionGeometry(const Scene &scene, JPH::BodyInterface &bodyInterface);

inline JPH::uint64 createBodyIdentifier(Handle handle, std::size_t transformIndex)
{
    assert(transformIndex <= std::numeric_limits<std::uint32_t>().max());
    return static_cast<JPH::uint64>(handle) << 32 | static_cast<JPH::uint64>(transformIndex);
}

inline BodyIdentifier getBodyIdentifier(JPH::uint64 bodyUserData)
{
    JPH::uint64 handle{bodyUserData >> 32};
    JPH::uint64 transformIndex{bodyUserData & 0x00000000FFFFFFFF};
    return {.handle = static_cast<Handle>(handle), .transformIndex = static_cast<std::uint32_t>(transformIndex)};
}

inline float getDensity(JPH::uint64 shapeUserData)
{
    const std::uint32_t lowBits{static_cast<std::uint32_t>(shapeUserData)};
    return std::bit_cast<float>(lowBits);
}

inline JPH::uint64 packDensity(float density)
{
    const std::uint32_t lowBits{std::bit_cast<std::uint32_t>(density)};
    return static_cast<JPH::uint64>(lowBits);
}

} // namespace physics