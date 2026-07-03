#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <string>
#include <variant>
#include <vector>

enum class AnimationPath : std::uint8_t
{
    translation,
    rotation,
    scale
};

enum class AnimationInterpolation : std::uint8_t
{
    step,
    linear
};

struct AnimationSampler
{
    int currentIndex{0};
    std::vector<float> times;
    std::vector<std::variant<glm::vec3, glm::quat>> values;
    AnimationInterpolation interpolation{AnimationInterpolation::linear};
};

struct AnimationChannel
{
    std::size_t samplerIndex{0};
    std::size_t targetNode{0};
    AnimationPath path{AnimationPath::translation};
};

struct Animation
{
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float start{std::numeric_limits<float>::infinity()};
    float end{-std::numeric_limits<float>::infinity()};
};