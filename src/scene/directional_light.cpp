#include "scene/directional_light.h"

#include <glm/glm.hpp>

DirectionalLight::DirectionalLight(const glm::vec3 &color, const glm::vec3 &direction)
    : color_{color}, direction_{glm::normalize(direction)}
{
}

const glm::vec3 &DirectionalLight::getColor() const
{
    return color_;
}

const glm::vec3 &DirectionalLight::getDirection() const
{
    return direction_;
}

void DirectionalLight::setDirection(const glm::vec3 &direction)
{
    direction_ = glm::normalize(direction);
    directionUpdated_ = true;
}

bool DirectionalLight::directionHasUpdated() const
{
    return directionUpdated_;
}

void DirectionalLight::setColor(const glm::vec3 &newColor)
{
    color_ = newColor;
    colorUpdated_ = true;
}

bool DirectionalLight::colorHasUpdated() const
{
    return colorUpdated_;
}

void DirectionalLight::clearFlags()
{
    colorUpdated_ = false;
    directionUpdated_ = false;
}