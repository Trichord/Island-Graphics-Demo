#include "models/animator.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <variant>
#include <vector>

#include "models/animation.h"
#include "models/primitive.h"
#include "models/skeleton.h"

namespace
{
std::vector<size_t> sortParents(const std::vector<int> &parents)
{
    std::vector<size_t> sorted;
    sorted.reserve(parents.size());
    std::vector<bool> seen(parents.size());

    for (std::size_t i{0}; i < parents.size(); ++i)
    {
        if (seen[i])
            continue;

        std::size_t current{i};
        std::vector<std::size_t> nodes;

        while (!seen[current])
        {
            nodes.push_back(current);
            seen[current] = true;

            if (parents[current] < 0)
                break;

            current = static_cast<std::size_t>(parents[current]);
        }

        std::reverse(nodes.begin(), nodes.end());

        for (std::size_t n : nodes)
            sorted.push_back(n);
    }

    return sorted;
}

glm::mat4 composeTRS(const glm::vec3 &t, const glm::quat &r, const glm::vec3 s)
{
    return glm::translate(glm::mat4{1.0f}, t) * glm::mat4_cast(r) * glm::scale(glm::mat4{1.0f}, s);
}

} // namespace

Animator::Animator(std::size_t numNodes, std::size_t numJoints)
    : localTranslations_(numNodes), localRotations_(numNodes), localScales_(numNodes), globalTransforms_(numNodes),
      jointMatrices_(numJoints)
{
}

void Animator::play(std::vector<Primitive> &primitives, const Skeleton &skeleton, Animation &animation, float deltaTime)
{
    time_ += deltaTime;

    const float duration{animation.end - animation.start};

    if (time_ > animation.end)
    {
        time_ = animation.start + std::fmod(time_ - animation.start, duration);

        for (AnimationSampler &sampler : animation.samplers)
            sampler.currentIndex = 0;
    }

    // Copy all local TRS to update with animations
    localTranslations_ = skeleton.restTranslations;
    localRotations_ = skeleton.restRotations;
    localScales_ = skeleton.restScales;

    for (const AnimationChannel &channel : animation.channels)
    {
        AnimationSampler &sampler{animation.samplers[channel.samplerIndex]};
        const std::variant<glm::vec3, glm::quat> TSorR{interpolateSampler(sampler)};

        switch (channel.path)
        {
            using enum AnimationPath;

        case translation:
            localTranslations_[channel.targetNode] = std::get<glm::vec3>(TSorR);
            break;

        case rotation:
            localRotations_[channel.targetNode] = std::get<glm::quat>(TSorR);
            break;

        case scale:
            localScales_[channel.targetNode] = std::get<glm::vec3>(TSorR);
            break;
        }
    }

    // Note: this is just for testing CPU side. After verifying the whole pipeline, the parents array will be sorted and
    // all node indices remapped at skeleton load time.
    std::vector<size_t> sortedParents{sortParents(skeleton.parents)};

    for (std::size_t i : sortedParents)
    {
        glm::mat4 localTRS{composeTRS(localTranslations_[i], localRotations_[i], localScales_[i])};

        if (skeleton.parents[i] < 0)
            globalTransforms_[i] = localTRS;
        else
            globalTransforms_[i] = globalTransforms_[skeleton.parents[i]] * localTRS;
    }

    // Write any non skeletal transforms back to the primitive's model space transform
    for (auto &[nodeIndex, primitiveIndices] : skeleton.nodeToPrimitive)
        for (std::size_t index : primitiveIndices)
            primitives[index].modelSpaceTransform = globalTransforms_[nodeIndex];

    // Update skinned joints for the SSBO
}

void Animator::reset(Animation &animation)
{
    time_ = 0.0f;

    for (AnimationSampler &sampler : animation.samplers)
        sampler.currentIndex = 0;
}

std::variant<glm::vec3, glm::quat> Animator::interpolateSampler(AnimationSampler &sampler)
{
    auto interpolate = [](AnimationInterpolation interpolation, const std::variant<glm::vec3, glm::quat> &v1,
                          const std::variant<glm::vec3, glm::quat> &v2, float t) {
        if (interpolation == AnimationInterpolation::linear)
        {
            if (auto tOrS1 = std::get_if<glm::vec3>(&v1))
                return std::variant<glm::vec3, glm::quat>{glm::mix(*tOrS1, std::get<glm::vec3>(v2), t)};
            else
                return std::variant<glm::vec3, glm::quat>{
                    glm::slerp(std::get<glm::quat>(v1), std::get<glm::quat>(v2), t)};
        }
        // Step
        else
        {
            if (auto tOrS1 = std::get_if<glm::vec3>(&v1))
                return std::variant<glm::vec3, glm::quat>{*tOrS1};
            else
                return std::variant<glm::vec3, glm::quat>{std::get<glm::quat>(v1)};
        }
    };

    // Don't start an animation before its first value
    if (time_ <= sampler.times.front())
    {
        std::variant<glm::vec3, glm::quat> &firstValue{sampler.values.front()};
        return interpolate(sampler.interpolation, firstValue, firstValue, 0.0f);
    }

    while (sampler.currentIndex + 1 < sampler.times.size() && time_ > sampler.times[sampler.currentIndex + 1])
        ++sampler.currentIndex;

    std::variant<glm::vec3, glm::quat> &currentValue{sampler.values[sampler.currentIndex]};

    // gLTF does not allow samplers to loop independently. The play function guarantees the full animation has not
    // gone past the animation's total end time
    if (sampler.currentIndex + 1 >= sampler.times.size())
        return interpolate(sampler.interpolation, currentValue, currentValue, 0.0f);

    float currentTime{sampler.times[sampler.currentIndex]};
    float nextTime{sampler.times[sampler.currentIndex + 1]};
    float t{(time_ - currentTime) / (nextTime - currentTime)};

    std::variant<glm::vec3, glm::quat> &nextValue{sampler.values[sampler.currentIndex + 1]};

    return interpolate(sampler.interpolation, currentValue, nextValue, t);
}