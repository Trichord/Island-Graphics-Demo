#include "renderer/cascade_shadow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "models/aabb.h"
#include "scene/directional_light.h"
#include "utilities/world_vectors.h"

CascadeShadow::CascadeShadow(const DirectionalLight &light, AABB worldAABB, float nearPlane) : nearPlane_{nearPlane}
{
    // Small adjustment so the furthest cascade perfectly fits
    constexpr float orthoBoxModifier{1.05f};

    const glm::vec3 center{worldAABB.getCenter()};
    const glm::vec3 min{worldAABB.getMin()};
    const glm::vec3 max{worldAABB.getMax()};
    const glm::vec3 width{max - min};

    const float radius{glm::length(width)};
    const float halfExtent{std::max({width.x, width.y}) * 0.5f * orthoBoxModifier};

    const glm::vec3 lightPos{center - light.getDirection() * radius};

    const glm::mat4 view{glm::lookAt(lightPos, center, WorldVectors::worldUp)};
    const glm::mat4 proj{glm::ortho(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.0f, 2.0f * radius)};

    worldRadius_ = radius;
    worldCenter_ = center;
    worldExtent_ = halfExtent * 2.0f;
    outerCascadeProjection_ = proj;
    cascades_.back().viewProjection = proj * view;
}

const Cascades &CascadeShadow::getCascades() const
{
    return cascades_;
}

/* Credit: Rouslan Dimitrov & NVIDIA Corporation
Computes the near and far distances for every frustum slice in camera eye space - that is, at what distance does a
slice start and end */
void CascadeShadow::calculateCascadeSplits(float farPlane)
{
    constexpr float cascadeOverlap{1.005f};
    constexpr float splitWeight{0.890315};

    float ratio{farPlane / nearPlane_};

    cascades_[0].near = nearPlane_;

    for (int i{1}; i < shadowCascadeCount; ++i)
    {
        float si{i / static_cast<float>(shadowCascadeCount)};

        cascades_[i].near = splitWeight * (nearPlane_ * std::pow(ratio, si)) +
                            (1 - splitWeight) * (nearPlane_ + (farPlane - nearPlane_) * si);
        cascades_[i - 1].far = cascades_[i].near * cascadeOverlap;
    }

    cascades_[shadowCascadeCount - 1].far = farPlane;
}

// Returns the bounding sphere of a frustum
CascadeShadow::FrustumSphere CascadeShadow::getFrustumSphere(const std::array<glm::vec3, 8> &corners,
                                                             int textureResolution)
{
    // Snaps radius to the nearest 1/amount to reduce shimmer
    float snapResolution{worldExtent_ / textureResolution};

    glm::vec3 center{0.0f};

    for (const auto &corner : corners)
        center += corner;

    center /= 8.0f;
    float radius{0.0f};

    for (const auto &corner : corners)
    {
        float dist{glm::length(corner - center)};
        radius = std::max(radius, dist);
    }

    return {center, std::ceil(radius * snapResolution) / snapResolution};
}

// Main technique: Antonio Hernandez Bejarano, texel snapping: Alex Tardiff
void CascadeShadow::updateCascadeShadows(const glm::mat4 &cameraVP, const DirectionalLight &light, int shadowResolution,
                                         float farPlane)
{
    // Pushes the radius for light projection near / far plane to avoid clipping geometry
    constexpr float nearFarMultiplier{2.39019f};

    const glm::mat4 inverseVPMatrix{glm::inverse(cameraVP)};
    const std::array<glm::vec3, 8> fullFrustumCorners{getFrustumCornersWorldSpace(inverseVPMatrix)};
    const glm::vec3 lightDir{glm::normalize(light.getDirection())};

    calculateCascadeSplits(farPlane);

    for (int i{0}; i < shadowCascadeCount - 1; ++i)
    {
        std::array<glm::vec3, 8> frustumCorners{fullFrustumCorners};
        Cascade &cascade{cascades_[i]};

        // Linearly interpolate to get the frustum split's near and far planes into [0,1]
        float splitDistance{(cascade.far - nearPlane_) / (farPlane - nearPlane_)};
        float lastSplitDist{(cascade.near - nearPlane_) / (farPlane - nearPlane_)};

        // Corners for this split
        for (int j{0}; j < 4; ++j)
        {
            glm::vec3 dist{frustumCorners[j + 4] - frustumCorners[j]};
            frustumCorners[j + 4] = glm::vec3{frustumCorners[j] + dist * splitDistance};
            frustumCorners[j] = glm::vec3{frustumCorners[j] + dist * lastSplitDist};
        }

        // Get center of frustum and texel density
        CascadeShadow::FrustumSphere frustumSphere{getFrustumSphere(frustumCorners, shadowResolution)};
        float texelsPerUnit{shadowResolution / (frustumSphere.radius * 2.0f)};

        // Create a base lookAt with our light direction
        glm::mat4 scalarMatrix{glm::scale(glm::mat4{1.0f}, glm::vec3{texelsPerUnit})};
        glm::mat4 lookAt{glm::lookAt(glm::vec3{0.0f}, -lightDir, WorldVectors::worldUp)};
        lookAt *= scalarMatrix;
        glm::mat4 invLookAt{glm::inverse(lookAt)};

        // Snap to the nearest texel in light space, then return to original space with inverse
        glm::vec3 frustumCenter{lookAt * glm::vec4{frustumSphere.center, 1.0f}};
        frustumCenter.x = std::floor(frustumCenter.x); // Clamp to texel
        frustumCenter.y = std::floor(frustumCenter.y); // Clamp to texel
        frustumCenter = invLookAt * glm::vec4{frustumCenter, 1.0f};

        // Final view projection matrix (near and far plane multiplied so that no objects are clipped)
        glm::vec3 eye{frustumCenter - (lightDir * frustumSphere.radius * 2.0f)};
        glm::mat4 lightView{glm::lookAt(eye, frustumCenter, WorldVectors::worldUp)};
        glm::mat4 lightProjection{glm::ortho(-frustumSphere.radius, frustumSphere.radius, -frustumSphere.radius,
                                             frustumSphere.radius, -frustumSphere.radius * nearFarMultiplier,
                                             frustumSphere.radius * nearFarMultiplier)};

        cascade.viewProjection = lightProjection * lightView;
    }

    const glm::vec3 lightPos{worldCenter_ - lightDir * worldRadius_};
    const glm::mat4 view{glm::lookAt(lightPos, worldCenter_, WorldVectors::worldUp)};
    cascades_.back().viewProjection = outerCascadeProjection_ * view;
}

std::array<glm::vec3, 8> CascadeShadow::getFrustumCornersWorldSpace(const glm::mat4 &invViewProjection)
{
    int index{0};
    std::array<glm::vec3, 8> frustumCorners;

    // 0-3 is near plane, 4-7 is far plane
    for (int z{0}; z < 2; z++)
        for (int x{0}; x < 2; ++x)
            for (int y{0}; y < 2; ++y)
            {
                glm::vec4 point{invViewProjection * glm::vec4{2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f}};
                frustumCorners[index++] = glm::vec3{point / point.w};
            }

    return frustumCorners;
}