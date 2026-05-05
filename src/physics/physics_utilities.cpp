#include "physics_utilities.h"

#include <algorithm>
#include <cstdint>
#include <span>

#include "glm/glm.hpp"
#include "jolt_impl.h"
#include "model.h"
#include "primitive.h"
#include "scene.h"
#include "transform.h"

void iterateVertices(physicsUtilities::CollisionMesh &collisionMesh, const std::span<Primitive> &primitives,
                     const glm::mat4 &modelMatrix)
{
    for (Primitive &primitive : primitives)
    {
        auto &vertices = primitive.vertices;
        auto &indices = primitive.indices;

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

physicsUtilities::CollisionMesh physicsUtilities::buildCollisionMesh(Scene &scene)
{
    physicsUtilities::CollisionMesh collisionMesh;

    for (SceneEntry &entry : scene.getAllSceneEntries())
    {
        Renderable &model{entry.renderableRef.get()};

        auto objectCollisionMesh = model.loadCollisionMesh();

        if (!objectCollisionMesh.has_value())
            continue;

        for (Transform &transform : entry.transforms)
            iterateVertices(collisionMesh, objectCollisionMesh.value(), transform.getModelMatrix());
    }

    return collisionMesh;
}