#pragma once

#include <vector>

#include "models/animation/skeleton.h"
#include "models/animation/animation.h"

struct SkeletalAnimation
{
    Skeleton skeleton;
    std::vector<Animation> animations;
};