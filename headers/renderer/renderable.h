#pragma once

#include <functional>
#include <span>
#include <utility>

#include "aabb.h"
#include "cull_method.h"
#include "glad/glad.h"
#include "primitive.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "transform.h"

struct Instance
{
    Transform transform;
    AABB modelAABB;
    AABB worldAABB;

    Instance() = default;

    template <typename T>
        requires std::constructible_from<Transform, T &&>
    Instance(T &&instanceTransform, const AABB &modelSpaceAABB)
        : transform{std::forward<T>(instanceTransform)}, modelAABB{modelSpaceAABB}
    {
        updateWorldAABB();
    }

    void updateWorldAABB()
    {
        glm::mat4 &modelMatrix{transform.getModelMatrix()};

        glm::mat3 RS{glm::mat3{modelMatrix}};
        glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

        glm::vec3 worldCenter{modelMatrix * glm::vec4(modelAABB.getCenter(), 1.0f)};
        glm::vec3 worldExtent{absRS * modelAABB.getExtent()};

        worldAABB = AABB{worldCenter - worldExtent, worldCenter + worldExtent};
    }
};

struct ObjectProperties
{
    std::vector<Instance> instances;

    //Culling
    bool isVisible{false};
    GLuint visibleInstances{0};
    CullMethod cullMethod{CullMethod::INSTANCED};

    // SSBO
    GLuint matricesBufferBaseIndex{0};
    GLuint indicesBufferIndex{0};
};

class IRenderable
{
  public:
    virtual AABB &getAABB() = 0;
    virtual void setMeshVisibility(bool visible) = 0;
    virtual const ModelHandle getHandle() const = 0;
    virtual std::span<Primitive::Mesh> getMeshes() = 0;
    virtual std::optional<std::vector<Primitive::Mesh>> loadCollisionMesh() = 0;
    virtual std::optional<std::vector<Instance>> generateInstancedData() = 0;

    virtual ~IRenderable() = default;
};

struct Renderable
{
    std::reference_wrapper<IRenderable> objectRef;
    ObjectProperties properties;
};