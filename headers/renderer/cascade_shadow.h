#pragma once

#include <array>

#include "directional_light.h"
#include "glm/glm.hpp"

class CascadeShadow
{
  public:
    static constexpr int cascadeCount{3};

    struct Split
    {
        float near{0.0f};
        float far{0.0f};
    };

    struct CascadeSplits
    {
        Split splits[cascadeCount];
    };

    struct LightMatrices
    {
        glm::mat4 viewProjections[cascadeCount];
    };

    CascadeShadow() = default;
    CascadeShadow(float nearPlane, float farPlane);

    const CascadeSplits &getSplitDistances() const;
    const LightMatrices &getLightMatrices() const;

    void updateCascadeShadows(const glm::mat4 &cameraViewProjectionMatrix, const DirectionalLight &light);

  private:
    struct FrustumSphere
    {
        glm::vec3 center;
        float radius;
    };

    float nearPlane_{0.0f};
    float farPlane_{0.0f};
    float range_{0.0f};
    float ratio_{0.0f};

    CascadeSplits frustumSplits_;
    LightMatrices lightVPMatrices_{};

    void calculateCascadeSplits();
    FrustumSphere getFrustumSphere(const std::array<glm::vec3, 8> &corners);
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection);
};
