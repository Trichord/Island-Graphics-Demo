// This software contains source code provided by NVIDIA Corporation.

#include <algorithm>
#include <array>
#include <cmath>

#include "cascade_shadow.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world_vectors.h"

static constexpr float maxShadowFarPlane{325.0f};

CascadeShadow::CascadeShadow(float nearPlane, float farPlane)
    : nearPlane_{nearPlane}, farPlane_{farPlane}, range_{farPlane - nearPlane}, ratio_{maxShadowFarPlane / nearPlane}
{
    calculateCascadeSplits();
}

// Credit: Rouslan Dimitrov & NVIDIA Corporation
/* Computes the near and far distances for every frustum slice in camera eye space - that is, at what distance does a
 * slice start and end */
void CascadeShadow::calculateCascadeSplits()
{
    constexpr float cascadeOverlap{1.005f};
    static constexpr float splitWeight_{0.6f};

    frustumSplits_.splits[0].near = nearPlane_;

    for (int i{1}; i < cascadeCount; ++i)
    {
        float si{i / static_cast<float>(cascadeCount)};

        frustumSplits_.splits[i].near = splitWeight_ * (nearPlane_ * std::pow(ratio_, si)) +
                                        (1 - splitWeight_) * (nearPlane_ + (maxShadowFarPlane - nearPlane_) * si);
        frustumSplits_.splits[i - 1].far = frustumSplits_.splits[i].near * cascadeOverlap;
    }

    frustumSplits_.splits[cascadeCount - 1].far = maxShadowFarPlane;
}

CascadeShadow::FrustumSphere CascadeShadow::getFrustumSphere(const std::array<glm::vec3, 8> &corners)
{
    static constexpr float snapResolution_{16.0f}; // Snaps the orthographic projection to the nearest amount

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

// Credit: Antonio Hernández Bejarano
void CascadeShadow::updateCascadeShadows(const glm::mat4 &cameraViewProjectionMatrix, const DirectionalLight &light)
{
    const glm::mat4 inverseVPMatrix{glm::inverse(cameraViewProjectionMatrix)};
    const std::array<glm::vec3, 8> fullFrustumCorners{getFrustumCornersWorldSpace(inverseVPMatrix)};
    const glm::vec3 lightDir{glm::normalize(light.getDirection())};

    for (int i{0}; i < cascadeCount; ++i)
    {
        std::array<glm::vec3, 8> frustumCorners{fullFrustumCorners};
        Split cascadeSplit{frustumSplits_.splits[i]};

        // Linearly interpolate to get the frustum split's near and far planes into [0,1]
        float splitDistance{(cascadeSplit.far - nearPlane_) / (farPlane_ - nearPlane_)};
        float lastSplitDist{(cascadeSplit.near - nearPlane_) / (farPlane_ - nearPlane_)};

        // Corners for this split
        for (int j{0}; j < 4; ++j)
        {
            glm::vec3 dist{frustumCorners[j + 4] - frustumCorners[j]};
            frustumCorners[j + 4] = glm::vec3{frustumCorners[j] + dist * splitDistance};
            frustumCorners[j] = glm::vec3{frustumCorners[j] + dist * lastSplitDist};
        }

        // Bounding sphere of the split
        CascadeShadow::FrustumSphere frustumSphere{getFrustumSphere(frustumCorners)};

        glm::vec3 maxExtent{frustumSphere.radius};
        glm::vec3 minExtent{-maxExtent};
        glm::vec3 eye{frustumSphere.center - lightDir * frustumSphere.radius};
        glm::mat4 lightView{glm::lookAt(eye, frustumSphere.center, WorldVectors::worldUp)};
        glm::mat4 lightProjection{
            glm::ortho(minExtent.x, maxExtent.x, minExtent.y, maxExtent.y, 0.0f, maxExtent.z - minExtent.z)};

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