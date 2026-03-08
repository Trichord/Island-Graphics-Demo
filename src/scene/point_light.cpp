#include "point_light.h"

#include "glm/glm.hpp"
#include "light.h"

PointLight::PointLight(const glm::vec3 &newPosition, const glm::vec3 &newColor) : Light{newColor}, position_{newPosition}
{
}

glm::vec3 PointLight::getColor() const
{
    return color_;
}

glm::vec3 PointLight::getPosition() const
{
    return position_;
}

void PointLight::setPosition(const glm::vec3 &newPosition)
{
    position_ = newPosition;
    positionUpdated_ = true;
}

bool PointLight::positionHasUpdated()
{
    if (positionUpdated_)
    {
        positionUpdated_ = false;
        return true;
    }
    else
        return false;
}