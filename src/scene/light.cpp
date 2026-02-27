#include "light.h"

#include "glm/glm.hpp"

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