#include "models/animation/animator.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <variant>
#include <vector>

#include "models/animation/animation.h"
#include "models/animation/skeleton.h"
#include "models/primitive.h"

namespace
{

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

void Animator::play(Animation &animation, const Skeleton &skeleton, std::vector<Primitive> &primitives, float deltaTime)
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

    // Pre-sorted so parents always come before children
    for (std::size_t i{0}; i < skeleton.parents.size(); ++i)
    {
        glm::mat4 localTRS{composeTRS(localTranslations_[i], localRotations_[i], localScales_[i])};

        if (skeleton.parents[i] < 0)
            globalTransforms_[i] = localTRS;
        else
            globalTransforms_[i] = globalTransforms_[skeleton.parents[i]] * localTRS;
    }

    // Write any non skeletal transforms back to the primitive's model space transform
    for (auto &[nodeIndex, primitiveIndices] : skeleton.nodeToPrimitives)
        for (std::size_t index : primitiveIndices)
            if (!primitives[index].vertexSkins.has_value())
                primitives[index].modelSpaceTransform = globalTransforms_[nodeIndex];

    // Update skinned joints for the SSBO
    if (!skeleton.skin.has_value())
        return;

    const Skeleton::Skin &skin{skeleton.skin.value()};

    for (std::size_t i{0}; i < skin.jointNodes.size(); ++i)
        jointMatrices_[i] = globalTransforms_[skin.jointNodes[i]] * skin.invBindMatrices[i];
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

const std::vector<glm::mat4> &Animator::getJointMatrices() const
{
    return jointMatrices_;
}