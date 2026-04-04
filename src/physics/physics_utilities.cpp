#include "physics_utilities.h"

#include <algorithm>
#include <cstdint>
#include <span>

#include "jolt/jolt.h"

#include "cull_method.h"
#include "glm/glm.hpp"
#include "jolt_impl.h"
#include "primitive.h"
#include "scene.h"

void iterateVertices(physicsUtilities::CollisionMesh &collisionMesh, const std::span<Primitive::Mesh> &meshes,
                     const glm::mat4 &modelMatrix)
{
    for (auto &mesh : meshes)
        for (auto &primitive : mesh.primitives)
        {
            auto vertices = primitive.getVertices();
            auto indices = primitive.getIndices();

            for (std::uint32_t i{0}; i < indices.size(); i += 3)
            {
                glm::vec3 a{vertices[indices[i]].position};
                glm::vec3 b{vertices[indices[i + 1]].position};
                glm::vec3 c{vertices[indices[i + 2]].position};

                a = glm::vec3{modelMatrix * glm::vec4{a, 1.0}};
                b = glm::vec3{modelMatrix * glm::vec4{b, 1.0}};
                c = glm::vec3{modelMatrix * glm::vec4{c, 1.0}};

                collisionMesh.minimumHeight = std::min({collisionMesh.minimumHeight, a.y, b.y, c.y});

                collisionMesh.mesh.emplace_back(toJPHFloat3(a), toJPHFloat3(b), toJPHFloat3(c));
            }
        }
}

physicsUtilities::CollisionMesh physicsUtilities::buildCollisionMesh(Scene& scene)
{
    using enum CullMethod;

    physicsUtilities::CollisionMesh collisionMesh;

    for (SceneEntry& sceneEntry : scene.getAllSceneEntries())
    {
            Renderable &renderable{sceneEntry.renderableRef.get()};

            auto objectCollisionMesh = renderable.loadCollisionMesh();
            auto meshes = renderable.getMeshes();

            for (auto &instance : sceneEntry.instances)
            {
                // Always use the terrain's actual mesh
                if (sceneEntry.cullMethod == TERRAIN)
                {
                    iterateVertices(collisionMesh, meshes, instance.transform.getModelMatrix());
                }
                // Otherwise, use the collision mesh if it exists
                else
                {
                    if (!objectCollisionMesh.has_value())
                        continue;

                    iterateVertices(collisionMesh, objectCollisionMesh.value(), instance.transform.getModelMatrix());
                }
            }
    }

    return collisionMesh;
}