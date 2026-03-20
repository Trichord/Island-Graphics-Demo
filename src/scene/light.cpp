#include "light.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

Light::Light(const glm::vec3 &newColor) : color_{newColor}
{
}

void Light::setColor(const glm::vec3 &newColor)
{
    color_ = newColor;
    colorUpdated_ = true;
}

bool Light::colorHasUpdated()
{
    if (colorUpdated_)
    {
        colorUpdated_ = false;
        return true;
    }
    else
        return false;
}

const glm::mat4 &Light::getProjectionMatrix() const
{
    return projectionMatrix_;
}

const glm::mat4 &Light::getViewProjectionMatrix() const
{
    return viewProjectionMatrix_;
}

void Light::setProjectionMatrix(float left, float right, float bottom, float top, float near, float far)
{
    projectionMatrix_ = glm::ortho(left, right, bottom, top, near, far);
}

void Light::setViewProjectionMatrix(const glm::vec3 &cameraPosition, const glm::vec3 &target, const glm::vec3 &worldUp)
{
    glm::mat4 viewMatrix{glm::lookAt(cameraPosition, target, worldUp)};
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix;
}