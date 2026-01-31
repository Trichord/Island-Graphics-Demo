#include "point_light.h"

#include "glm/glm.hpp"
#include "light.h"

PointLight::PointLight(const glm::vec3 &newPosition, const glm::vec3 &newColor) : Light{newColor}, position{newPosition}
{
}

glm::vec3 PointLight::getColor() const
{
    return color;
}

glm::vec3 PointLight::getPosition() const
{
    return position;
}

void PointLight::setPosition(const glm::vec3 &newPosition)
{
    position = newPosition;
    positionUpdated = true;
}

bool PointLight::positionHasUpdated()
{
    if (positionUpdated)
    {
        positionUpdated = false;
        return true;
    }
    else
        return false;
}