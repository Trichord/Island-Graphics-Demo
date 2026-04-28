#pragma once

#include <array>

#include "directional_light.h"
#include "glm/glm.hpp"

struct Cascade
{
    float near{0.0f};
    float far{0.0f};
    glm::mat4 viewProjection{1.0f};
};

inline constexpr int shadowCascadeCount{3};
using Cascades = std::array<Cascade, shadowCascadeCount>;

class CascadeShadow
{
  public:
    CascadeShadow() = default;
    CascadeShadow(float nearPlane);

    [[nodiscard]] const Cascades &updateCascadeShadows(const glm::mat4 &cameraVP, const DirectionalLight &light,
                                                       int shadowResolution, float farPlane);

  private:
    struct FrustumSphere
    {
        glm::vec3 center;
        float radius;
    };

    float nearPlane_{0.0f};
    std::array<Cascade, shadowCascadeCount> cascades_;

    void calculateCascadeSplits(float farPlane);
    FrustumSphere getFrustumSphere(const std::array<glm::vec3, 8> &corners, int shadowResolution);
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection);
};
