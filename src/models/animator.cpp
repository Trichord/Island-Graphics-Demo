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

void Animator::setAnimation(Animation &animation)
{
    if (currentAnimation_ == &animation && !nextAnimation_)
        return;

    currentAnimation_ = &animation;
    resetSamplers(*currentAnimation_);
    resetBlend();
}

void Animator::setTransition(Animation &blendTo, float blendDuration)
{
    if (nextAnimation_ == &blendTo)
        return;

    if (currentAnimation_ == &blendTo && !nextAnimation_)
        return;

    resetBlend();
    resetSamplers(blendTo);
    nextAnimation_ = &blendTo;
    blendDuration_ = blendDuration;
}

void Animator::resetBlend()
{
    nextAnimation_ = nullptr;
    blendTime_ = 0.0f;
    blendDuration_ = 0.0f;
}

void Animator::resetSamplers(Animation &animation)
{
    for (AnimationSampler &sampler : animation.samplers)
        sampler.currentIndex = 0;
}

void Animator::sampleChannels(Animation &animation, std::vector<glm::vec3> &translations,
                              std::vector<glm::quat> &rotations, std::vector<glm::vec3> &scales, float time)
{
    for (const AnimationChannel &channel : animation.channels)
    {
        AnimationSampler &sampler{animation.samplers[channel.samplerIndex]};
        const std::variant<glm::vec3, glm::quat> TSorR{interpolateSampler(sampler, time)};

        switch (channel.path)
        {
            using enum AnimationPath;

        case translation:
            translations[channel.targetNode] = std::get<glm::vec3>(TSorR);
            break;

        case rotation:
            rotations[channel.targetNode] = std::get<glm::quat>(TSorR);
            break;

        case scale:
            scales[channel.targetNode] = std::get<glm::vec3>(TSorR);
            break;
        }
    }
}

void Animator::update(const Skeleton &skeleton, std::vector<Primitive> &primitives, float deltaTime)
{
    if (!currentAnimation_)
        return;

    time_ += deltaTime;

    const float duration{currentAnimation_->end - currentAnimation_->start};

    if (time_ > currentAnimation_->end)
    {
        time_ = currentAnimation_->start + std::fmod(time_ - currentAnimation_->start, duration);

        for (AnimationSampler &sampler : currentAnimation_->samplers)
            sampler.currentIndex = 0;
    }

    // Copy all rest TRS, for any TRS not targeted by the animation
    localTranslations_ = skeleton.restTranslations;
    localRotations_ = skeleton.restRotations;
    localScales_ = skeleton.restScales;

    sampleChannels(*currentAnimation_, localTranslations_, localRotations_, localScales_, time_);

    if (nextAnimation_ && blendTime_ < blendDuration_)
    {
        const float nextDuration{nextAnimation_->end - nextAnimation_->start};
        blendTime_ = blendTime_ + deltaTime;
        const float blend{std::clamp(blendTime_ / blendDuration_, 0.0f, 1.0f)};

        nextLocalTranslations_ = skeleton.restTranslations;
        nextLocalRotations_ = skeleton.restRotations;
        nextLocalScales_ = skeleton.restScales;

        sampleChannels(*nextAnimation_, nextLocalTranslations_, nextLocalRotations_, nextLocalScales_,
                       std::clamp(blendTime_, 0.0f, nextDuration));

        for (std::size_t i{0}; i < nextLocalTranslations_.size(); ++i)
        {
            localTranslations_[i] = glm::mix(localTranslations_[i], nextLocalTranslations_[i], blend);
            localRotations_[i] = glm::slerp(localRotations_[i], nextLocalRotations_[i], blend);
            localScales_[i] = glm::mix(localScales_[i], nextLocalScales_[i], blend);
        }

        if (blendTime_ >= blendDuration_)
        {
            currentAnimation_ = nextAnimation_;

            time_ = blendTime_;
            resetBlend();
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

// Interpolates sampler values. The caller must guarantee the full animation has not gone past the animation's total end
// time
std::variant<glm::vec3, glm::quat> Animator::interpolateSampler(AnimationSampler &sampler, float time)
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
    if (time <= sampler.times.front())
    {
        std::variant<glm::vec3, glm::quat> &firstValue{sampler.values.front()};
        return interpolate(sampler.interpolation, firstValue, firstValue, 0.0f);
    }

    while (sampler.currentIndex + 1 < sampler.times.size() && time > sampler.times[sampler.currentIndex + 1])
        ++sampler.currentIndex;

    std::variant<glm::vec3, glm::quat> &currentValue{sampler.values[sampler.currentIndex]};

    // gLTF does not allow samplers to loop independently.
    if (sampler.currentIndex + 1 >= sampler.times.size())
        return interpolate(sampler.interpolation, currentValue, currentValue, 0.0f);

    float currentTime{sampler.times[sampler.currentIndex]};
    float nextTime{sampler.times[sampler.currentIndex + 1]};
    float t{(time - currentTime) / (nextTime - currentTime)};

    std::variant<glm::vec3, glm::quat> &nextValue{sampler.values[sampler.currentIndex + 1]};

    return interpolate(sampler.interpolation, currentValue, nextValue, t);
}

const std::vector<glm::mat4> &Animator::getJointMatrices() const
{
    return jointMatrices_;
}