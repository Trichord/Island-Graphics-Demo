// This software contains source code provided by NVIDIA Corporation.

#include <algorithm>
#include <array>
#include <cmath>

#include "cascade_shadow.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world_vectors.h"

CascadeShadow::CascadeShadow(float nearPlane) : nearPlane_{nearPlane}
{
}

/* Credit: Rouslan Dimitrov & NVIDIA Corporation
Computes the near and far distances for every frustum slice in camera eye space - that is, at what distance does a
slice start and end */
void CascadeShadow::calculateCascadeSplits(float farPlane)
{
    constexpr float cascadeOverlap{1.005f};
    constexpr float splitWeight_{0.8f};

    float ratio{farPlane / nearPlane_};

    frustumSplits_.splits[0].near = nearPlane_;

    for (int i{1}; i < cascadeCount; ++i)
    {
        float si{i / static_cast<float>(cascadeCount)};

        frustumSplits_.splits[i].near = splitWeight_ * (nearPlane_ * std::pow(ratio, si)) +
                                        (1 - splitWeight_) * (nearPlane_ + (farPlane - nearPlane_) * si);
        frustumSplits_.splits[i - 1].far = frustumSplits_.splits[i].near * cascadeOverlap;
    }

    frustumSplits_.splits[cascadeCount - 1].far = farPlane;
}

// Returns the bounding sphere of a frustum
CascadeShadow::FrustumSphere CascadeShadow::getFrustumSphere(const std::array<glm::vec3, 8> &corners)
{
    // Snaps radius to the nearest 1/16th to reduce shimmer
    constexpr float snapResolution_{16.0f}; 

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

    return {center, std::ceil(radius * snapResolution_) / snapResolution_};
}

// Main technique: Antonio Hernandez Bejarano, texel snapping: Alex Tardiff
void CascadeShadow::updateCascadeShadows(const glm::mat4 &cameraViewProjectionMatrix, const DirectionalLight &light,
                                         float farPlane)
{
    //Pushes the radius for light projection near / far plane to avoid clipping geometry
    constexpr float radiusMultiplier{2.75f};

    const glm::mat4 inverseVPMatrix{glm::inverse(cameraViewProjectionMatrix)};
    const std::array<glm::vec3, 8> fullFrustumCorners{getFrustumCornersWorldSpace(inverseVPMatrix)};
    const glm::vec3 lightDir{glm::normalize(light.getDirection())};

    calculateCascadeSplits(farPlane);

    for (int i{0}; i < cascadeCount; ++i)
    {
        std::array<glm::vec3, 8> frustumCorners{fullFrustumCorners};
        Split cascadeSplit{frustumSplits_.splits[i]};

        // Linearly interpolate to get the frustum split's near and far planes into [0,1]
        float splitDistance{(cascadeSplit.far - nearPlane_) / (farPlane - nearPlane_)};
        float lastSplitDist{(cascadeSplit.near - nearPlane_) / (farPlane - nearPlane_)};

        // Corners for this split
        for (int j{0}; j < 4; ++j)
        {
            glm::vec3 dist{frustumCorners[j + 4] - frustumCorners[j]};
            frustumCorners[j + 4] = glm::vec3{frustumCorners[j] + dist * splitDistance};
            frustumCorners[j] = glm::vec3{frustumCorners[j] + dist * lastSplitDist};
        }

        // Get center of frustum and texel density
        CascadeShadow::FrustumSphere frustumSphere{getFrustumSphere(frustumCorners)};
        float texelsPerUnit{1024.0f / (frustumSphere.radius * 2.0f)};

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
                                             frustumSphere.radius, -frustumSphere.radius * radiusMultiplier,
                                             frustumSphere.radius * radiusMultiplier)};

        lightVPMatrices_.viewProjections[i] = lightProjection * lightView;
    }
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

const CascadeShadow::CascadeSplits &CascadeShadow::getSplitDistances() const
{
    return frustumSplits_;
}

const CascadeShadow::LightMatrices &CascadeShadow::getLightMatrices() const
{
    return lightVPMatrices_;
}