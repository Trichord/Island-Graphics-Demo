#include "physics/physics_utilities.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/EActivation.h>
#include <algorithm>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <variant>
#include <vector>

#include "models/primitive.h"
#include "physics/collision_shapes.h"
#include "physics/jolt_impl.h"
#include "scene/scene.h"
#include "utilities/asset_loader.h"
#include "utilities/overloaded.h"
#include "utilities/transform.h"

namespace
{

JPH::TriangleList buildMesh(physics::StaticBodies &staticBodies, const std::vector<Primitive> &primitives)
{
    JPH::TriangleList mesh;

    for (const Primitive &primitive : primitives)
    {
        auto &vertices = primitive.vertices;
        auto &indices = primitive.indices;

        for (std::uint32_t i{0}; i < indices.size(); i += 3)
        {
            glm::vec3 a{vertices[indices[i]].position};
            glm::vec3 b{vertices[indices[i + 1]].position};
            glm::vec3 c{vertices[indices[i + 2]].position};

            a = glm::vec3{primitive.modelSpaceTransform * glm::vec4{a, 1.0}};
            b = glm::vec3{primitive.modelSpaceTransform * glm::vec4{b, 1.0}};
            c = glm::vec3{primitive.modelSpaceTransform * glm::vec4{c, 1.0}};

            mesh.emplace_back(toJPHFloat3(a), toJPHFloat3(b), toJPHFloat3(c));
        }
    }

    return mesh;
}

void addBody(physics::StaticBodies &bodies, JPH::BodyInterface &interface, const JPH::Ref<JPH::Shape> &shape,
             const Transform &transform)
{
    const JPH::RVec3 pos{toJPHVec3(transform.getPosition())};
    const JPH::Quat rotation{toJPHQuat(transform.getRotation())};
    const JPH::Vec3 scale{toJPHVec3(transform.getScale())};
    const JPH::ShapeRefC scaledShape{new JPH::ScaledShape(shape, scale)};

    JPH::BodyCreationSettings settings{scaledShape, pos, rotation, JPH::EMotionType::Static, Layers::NON_MOVING};

    JPH::BodyID id{interface.CreateAndAddBody(settings, JPH::EActivation::DontActivate)};

    JPH::AABox bounds{
        scaledShape->GetWorldSpaceBounds(JPH::Mat44::sRotationTranslation(rotation, pos), JPH::Vec3::sReplicate(1.0f))};

    bodies.minimumHeight = std::min(bodies.minimumHeight, bounds.mMin.GetY());
    bodies.ids.emplace_back(id);
}
} // namespace

physics::StaticBodies physics::buildCollisionGeometry(const Scene &scene, JPH::BodyInterface &interface)
{
    StaticBodies bodies;

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        if (!entry.collisionGeometry)
            continue;

        std::visit(overloaded{[&](const Collision::Mesh &mesh) {
                                  fastgltf::Asset asset{loader::loadAsset(mesh.path)};
                                  const std::vector<Primitive> primitives{loader::loadMeshes(asset)};
                                  JPH::TriangleList collisionMesh{buildMesh(bodies, primitives)};

                                  if (collisionMesh.empty())
                                      return;

                                  auto shape = JPH::MeshShapeSettings{collisionMesh}.Create().Get();

                                  for (const Transform &transform : entry.worldTransforms)
                                      addBody(bodies, interface, shape, transform);
                              },
                              [&](const Collision::Capsule capsule) {
                                  auto shape = JPH::CapsuleShapeSettings(
                                                   (capsule.height - 2.0f * capsule.radius) * 0.5f, capsule.radius)
                                                   .Create()
                                                   .Get();

                                  for (const Transform &transform : entry.worldTransforms)
                                      addBody(bodies, interface, shape, transform);
                              }},
                   *entry.collisionGeometry);
    }

    return bodies;
}