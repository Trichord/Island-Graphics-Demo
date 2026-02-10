#include "directional_light.h"

#include "glm/glm.hpp"
#include "light.h"

DirectionalLight::DirectionalLight(const glm::vec3 &newDirection, const glm::vec3 &newColor)
    : Light{newColor}, direction{newDirection}
{
}

glm::vec3 DirectionalLight::getColor() const
{
    return color;
}

glm::vec3 DirectionalLight::getDirection() const
{
    return direction;
}

void DirectionalLight::setDirection(const glm::vec3 &newDirection)
{
    direction = newDirection;
    directionUpdated = true;
}

bool DirectionalLight::directionHasUpdated()
{
    if (directionUpdated)
    {
        directionUpdated = false;
        return true;
    }
    else
        return false;
}