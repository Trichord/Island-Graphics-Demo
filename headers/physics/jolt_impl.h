#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>

static inline glm::vec3 toGLMVec3(const JPH::Vec3 &vec)
{
    return {vec.GetX(), vec.GetY(), vec.GetZ()};
}

static inline glm::quat toGLMQuat(const JPH::Quat &quat)
{
    return {quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ()};
}

static inline JPH::Vec3 toJPHVec3(const glm::vec3 &vec)
{
    return {vec.x, vec.y, vec.z};
}

static inline JPH::Float3 toJPHFloat3(const glm::vec3 &vec)
{
    return {vec.x, vec.y, vec.z};
}

static inline JPH::Quat toJPHQuat(const glm::quat &quat)
{
    return {quat.x, quat.y, quat.z, quat.w};
}

namespace Layers
{
static constexpr JPH::ObjectLayer NON_MOVING{0};
static constexpr JPH::ObjectLayer MOVING{1};
static constexpr JPH::ObjectLayer NUM_LAYERS{2};
}; // namespace Layers

namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING{0};
static constexpr JPH::BroadPhaseLayer MOVING{1};
static constexpr JPH::uint NUM_LAYERS{2};
}; // namespace BroadPhaseLayers

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1)
        {
        case Layers::NON_MOVING:
            return layer2 == Layers::MOVING; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            throw std::runtime_error("ERROR: Unknown Jolt ObjectLayer");
        }
    }
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
  public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        objectToBroadPhase_[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        objectToBroadPhase_[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        if (layer >= Layers::NUM_LAYERS)
            throw std::runtime_error("ERROR: Unknown Jolt ObjectLayer");

        return objectToBroadPhase_[layer];
    }

  private:
    JPH::BroadPhaseLayer objectToBroadPhase_[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
    {
        switch (layer1)
        {
        case Layers::NON_MOVING:
            return layer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            throw std::runtime_error("ERROR: Uknown Jolt ObjectVsObject layer");
        }
    }
};