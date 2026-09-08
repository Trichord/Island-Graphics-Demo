#pragma once

#include <array>
#include <glm/glm.hpp>

#include "models/aabb.h"
#include "scene/directional_light.h"

class CascadeShadow
{
  public:
    static constexpr int cascadeCount{3};

    struct Cascades
    {
        glm::vec4 nearPlanes{0.0f};
        glm::vec4 farPlanes{0.0f};
        glm::vec4 texelSizes{0.0f};
        std::array<glm::mat4, cascadeCount> viewProjections{glm::mat4{1.0f}, glm::mat4{1.0f}, glm::mat4{1.0f}};
    };

    CascadeShadow() = default;
    CascadeShadow(const DirectionalLight &light, AABB worldAABB, float nearPlane);

    const Cascades &getCascades() const;

    void updateCascadeShadows(const glm::mat4 &cameraVP, const DirectionalLight &light, int shadowResolution,
                              float farPlane);

  private:
    struct FrustumSphere
    {
        glm::vec3 center;
        float radius;
    };

    float nearPlane_{0.0f};
    float worldRadius_{0.0f};
    float worldExtent_{0.0f};
    glm::vec3 worldCenter_{0.0f};
    glm::mat4 outerCascadeProjection_{1.0f};
    Cascades cascades_;

    void calculateCascadeSplits(float farPlane);
    FrustumSphere getFrustumSphere(const std::array<glm::vec3, 8> &corners, int shadowResolution);
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection);
};
