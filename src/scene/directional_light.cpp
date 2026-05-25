#include "scene/directional_light.h"

#include <glm/glm.hpp>

#include "scene/light.h"

DirectionalLight::DirectionalLight(const glm::vec3 &newDirection, const glm::vec3 &newColor)
    : Light{newColor}, direction_{newDirection}
{
}

glm::vec3 DirectionalLight::getColor() const
{
    return color_;
}

glm::vec3 DirectionalLight::getDirection() const
{
    return direction_;
}

void DirectionalLight::setDirection(const glm::vec3 &newDirection)
{  
    direction_ = newDirection;
    directionUpdated_ = true;
}

bool DirectionalLight::directionHasUpdated()
{
    if (directionUpdated_)
    {
        directionUpdated_ = false;
        return true;
    }
    else
        return false;
}