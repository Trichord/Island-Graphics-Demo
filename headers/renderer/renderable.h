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

struct SceneEntry;

struct BoundedTransform
{
    Transform transform;
    AABB worldAABB;

    BoundedTransform() = default;

    template <typename T>
        requires std::constructible_from<Transform, T &&>
    BoundedTransform(T &&instanceTransform, const AABB &modelSpaceAABB) : transform{std::forward<T>(instanceTransform)}
    {
        updateWorldAABB(modelSpaceAABB);
    }

    void updateWorldAABB(const AABB &modelAABB)
    {
        glm::mat4 &modelMatrix{transform.getModelMatrix()};

        glm::mat3 RS{glm::mat3{modelMatrix}};
        glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

        glm::vec3 worldCenter{modelMatrix * glm::vec4(modelAABB.getCenter(), 1.0f)};
        glm::vec3 worldExtent{absRS * modelAABB.getExtent()};

        worldAABB = AABB{worldCenter - worldExtent, worldCenter + worldExtent};
    }
};

class Renderable
{
  public:
    virtual AABB &getAABB() = 0;
    virtual void setMeshVisibility(bool visible) = 0;
    virtual ModelHandle getHandle() const = 0;
    virtual std::span<Primitive::Mesh> getMeshes() = 0;
    virtual std::optional<std::vector<Primitive::Mesh>> loadCollisionMesh() = 0;
    virtual std::optional<std::vector<BoundedTransform>> loadInstancedTransforms() = 0;

    virtual ~Renderable() = default;
};

struct DrawEntry
{
    // Renderable object
    std::reference_wrapper<Renderable> renderableRef;
    std::reference_wrapper<std::vector<BoundedTransform>> transformsRef;

    // Drawing
    ShaderType shaderType{ShaderType::DIRECTIONAL_PBR};

    // Culling
    bool isVisible{true};
    int visibleInstances{static_cast<int>(transformsRef.get().size())};
    CullMethod cullMethod{CullMethod::INSTANCED};

    // SSBO
    int indicesBufferIndex{0};
};