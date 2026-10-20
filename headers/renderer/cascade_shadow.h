#pragma once

#include <array>
#include <glm/glm.hpp>

#include "models/aabb.h"
#include "scene/directional_light.h"

/**
 * @brief Handles all the frustum splitting and light matrix generation.
 */
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

    /**
    * @brief Main technique: Antonio Hernandez Bejarano, texel snapping: Alex Tardiff
    *
    * Calculate new near/far splits and light matrices.
    */
    void updateCascadeShadows(const glm::mat4 &cameraVP, const DirectionalLight &light, int shadowResolution,
                              float farPlane);

  private:
    struct FrustumSphere
    {
        glm::vec3 center{0.0f};
        float radius{0.0f};
    };

    float nearPlane_{0.0f};
    float worldRadius_{0.0f};
    float worldExtent_{0.0f};
    glm::vec3 worldCenter_{0.0f};
    glm::mat4 outerCascadeProjection_{1.0f};
    Cascades cascades_;

    /**
    * @brief Credit: Rouslan Dimitrov & NVIDIA Corporation.
    * 
    * Computes the near and far distances for every frustum slice in camera eye space - that is, at what distance does a
    * slice start and end.
    */
    void calculateCascadeSplits(float farPlane);

    /**
    * @brief Returns the bounding sphere of a frustum.
    */
    FrustumSphere getFrustumSphere(const std::array<glm::vec3, 8> &corners, int shadowResolution);

    /**
    * @brief Returns the world space corners of a frustum. 0-3 are the near plane, 4-7 are the far plane.
    */
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection);
};
