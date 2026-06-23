#pragma once

#include <glm/glm.hpp>
#include <variant>
#include <vector>

#include "models/animation.h"
#include "models/primitive.h"
#include "models/skeleton.h"

class Animator
{
  public:
    Animator() = default;
    Animator(std::size_t numNodes, std::size_t numJoints);

    void play(Animation &animation, const Skeleton &skeleton, std::vector<Primitive> &primitives, float deltaTime);

    void reset(Animation &animation);

  private:
    float time_{0.0f};

    std::vector<glm::vec3> localTranslations_;
    std::vector<glm::quat> localRotations_;
    std::vector<glm::vec3> localScales_;

    // Recalculate each frame from animation changes
    std::vector<glm::mat4> globalTransforms_;

    // Pass to shaders
    std::vector<glm::mat4> jointMatrices_;

    std::variant<glm::vec3, glm::quat> interpolateSampler(AnimationSampler &sampler);
};