#include "physics/physics_utilities.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Array.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/EActivation.h>
#include <algorithm>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <iostream>
#include <variant>
#include <vector>

#include "models/primitive.h"
#include "physics/collision_shapes.h"
#include "physics/jolt_impl.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/asset_loader.h"
#include "utilities/overloaded.h"
#include "utilities/transform.h"

namespace
{
JPH::TriangleList buildMesh(const std::vector<Primitive> &primitives)
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

            a = glm::vec3{primitive.modelSpaceTransform * glm::vec4{a, 1.0f}};
            b = glm::vec3{primitive.modelSpaceTransform * glm::vec4{b, 1.0f}};
            c = glm::vec3{primitive.modelSpaceTransform * glm::vec4{c, 1.0f}};

            mesh.emplace_back(toJPHFloat3(a), toJPHFloat3(b), toJPHFloat3(c));
        }
    }

    return mesh;
}

JPH::Array<JPH::Vec3> buildPoints(const std::vector<Primitive> &primitives)
{
    std::size_t numVerts{0};

    for (const Primitive &p : primitives)
        numVerts += p.vertices.size();

    JPH::Array<JPH::Vec3> points;
    points.reserve(numVerts);

    for (const Primitive &primitive : primitives)
        for (const Primitive::Vertex &vertex : primitive.vertices)
        {
            const glm::vec3 transformedVert{primitive.modelSpaceTransform * glm::vec4{vertex.position, 1.0f}};
            points.emplace_back(toJPHVec3(transformedVert));
        }

    return points;
}

void addBody(physics::StaticBodies &bodies, JPH::BodyInterface &interface, const JPH::Ref<JPH::Shape> &shape,
             const Transform &transform, Handle handle, std::size_t transformIndex, float friction, bool dynamic)
{
    const JPH::RVec3 pos{toJPHVec3(transform.getPosition())};
    const JPH::Quat rotation{toJPHQuat(transform.getRotation())};
    const JPH::Vec3 scale{toJPHVec3(transform.getScale())};
    const JPH::ShapeRefC scaledShape{new JPH::ScaledShape(shape, scale)};

    JPH::BodyCreationSettings settings{scaledShape, pos, rotation,
                                       dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                       dynamic ? Layers::MOVING : Layers::NON_MOVING};
    settings.mUserData = physics::createBodyIdentifier(handle, transformIndex);
    settings.mFriction = friction;

    JPH::BodyID id{
        interface.CreateAndAddBody(settings, dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate)};

    JPH::AABox bounds{interface.GetTransformedShape(id).GetWorldSpaceBounds()};
    bodies.minimumHeight = std::min(bodies.minimumHeight, bounds.mMin.GetY());
    bodies.ids.emplace_back(id);
}
} // namespace

physics::StaticBodies physics::buildCollisionGeometry(const Scene &scene, JPH::BodyInterface &bodyInterface)
{
    StaticBodies bodies;

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        if (!entry.collisionGeometry)
            continue;

        std::visit(overloaded{[&](const collision::Mesh &mesh) {
                                  fastgltf::Asset asset{loader::loadAsset(mesh.path)};
                                  const std::vector<Primitive> primitives{loader::loadMeshes(asset)};
                                  JPH::TriangleList collisionMesh{buildMesh(primitives)};

                                  if (collisionMesh.empty())
                                      return;

                                  auto shape = JPH::MeshShapeSettings{collisionMesh}.Create().Get();

                                  for (std::size_t i{0}; i < entry.worldTransforms.size(); ++i)
                                      addBody(bodies, bodyInterface, shape, entry.worldTransforms[i], entry.handle, i,
                                              mesh.material.friction, false);
                              },

                              [&](const collision::ConvexHull &hull) {
                                  fastgltf::Asset asset{loader::loadAsset(hull.path)};
                                  const std::vector<Primitive> primitives{loader::loadMeshes(asset)};
                                  JPH::Array<JPH::Vec3> points{buildPoints(primitives)};

                                  JPH::ConvexHullShapeSettings convexHullSettings{points};
                                  convexHullSettings.mDensity = hull.material.density;
                                  JPH::ShapeSettings::ShapeResult result{convexHullSettings.Create()};

                                  if (result.HasError())
                                  {
                                      std::cerr << "Error creating convex hull: " << result.GetError() << std::endl;
                                      return;
                                  }

                                  for (std::size_t i{0}; i < entry.worldTransforms.size(); ++i)
                                      addBody(bodies, bodyInterface, result.Get(), entry.worldTransforms[i],
                                              entry.handle, i, hull.material.friction, hull.dynamic);
                              },
                              [&](const collision::Capsule &capsule) {
                                  JPH::CapsuleShapeSettings shapeSettings{
                                      (capsule.height - 2.0f * capsule.radius) * 0.5f, capsule.radius};
                                  shapeSettings.mDensity = capsule.material.density;

                                  JPH::ShapeSettings::ShapeResult result{shapeSettings.Create()};

                                  // Add a model space transform before world space transform
                                  auto offsetShape{new JPH::RotatedTranslatedShape(
                                      toJPHVec3(capsule.offset), JPH::Quat::sIdentity(), result.Get())};

                                  for (std::size_t i{0}; i < entry.worldTransforms.size(); ++i)
                                      addBody(bodies, bodyInterface, offsetShape, entry.worldTransforms[i],
                                              entry.handle, i, capsule.material.friction, capsule.dynamic);
                              }},
                   *entry.collisionGeometry);
    }

    return bodies;
}