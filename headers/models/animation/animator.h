#pragma once

#include <glm/glm.hpp>
#include <variant>
#include <vector>

#include "models/animation/animation.h"
#include "models/animation/node_graph.h"
#include "models/animation/skeleton.h"
#include "models/primitive.h"

class Animator
{
  public:
    Animator() = default;
    Animator(NodeGraph &&nodeGraph, std::size_t numJoints);

    void setAnimation(Animation &animation);
    void setTransition(Animation &transitionTo, float blendDuration);
    void update(std::vector<Primitive> &primitives, const std::optional<Skeleton> &skeleton, float deltaTime);

    const std::vector<glm::mat4> &getJointMatrices() const;

  private:
    NodeGraph nodeGraph_;

    float time_{0.0f};
    float blendTime_{0.0f};
    float blendDuration_{0.0f};

    Animation *currentAnimation_{nullptr};
    Animation *nextAnimation_{nullptr};

    // Scratch space for accumulating new animations. Avoids heap allocations every frame.
    std::vector<glm::vec3> localTranslations_;
    std::vector<glm::quat> localRotations_;
    std::vector<glm::vec3> localScales_;

    // Scratch space for blending two animations.
    std::vector<glm::vec3> nextLocalTranslations_;
    std::vector<glm::quat> nextLocalRotations_;
    std::vector<glm::vec3> nextLocalScales_;

    // Recalculate each frame from animation changes.
    std::vector<glm::mat4> globalTransforms_;

    // Uploaded to GPU each frame.
    std::vector<glm::mat4> jointMatrices_;

    std::variant<glm::vec3, glm::quat> interpolateSampler(AnimationSampler &sampler, float time);
    void sampleChannels(Animation &animation, std::vector<glm::vec3> &translations, std::vector<glm::quat> &rotations,
                        std::vector<glm::vec3> &scales, float time);
    void resetSamplers(Animation &animation);
    void resetBlend();
};