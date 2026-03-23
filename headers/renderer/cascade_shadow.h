#pragma once

#include <array>

#include "directional_light.h"
#include "glm/glm.hpp"

class CascadeShadow
{
  public:
    static constexpr int cascadeCount{3};

    struct SplitDistance
    {
        float near;
        float far;
    };

    CascadeShadow() = default;
    CascadeShadow(const glm::mat4 &cameraViewMatrix, const DirectionalLight &light, float nearPlane, float farPlane);

    const std::array<SplitDistance, cascadeCount> &getSplitDistances() const;
    const std::array<glm::mat4, cascadeCount> &getLightVPMatrices() const;

    void updateCascadeShadows(const glm::mat4 cameraViewMatrix, const glm::mat4 &cameraProjMatrix,
                              const DirectionalLight &light);

  private:
    struct FrustumSphere
    {
        glm::vec3 center;
        float radius;
    };

    float minZ_{0.0f};
    float maxZ_{0.0f};
    float range_{0.0f};
    float ratio_{0.0f};

    std::array<SplitDistance, cascadeCount> cascadeSplits_;
    std::array<glm::mat4, cascadeCount> lightVPMatrices_;

    void calculateCascadeSplits();
    FrustumSphere getFrustumSphere(const std::array<glm::vec3, 8> corners);
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection);
};
