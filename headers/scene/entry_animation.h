#pragma once

#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "models/animation/animation.h"
#include "models/animation/animator.h"
#include "models/animation/skeleton.h"
#include "models/primitive.h"
#include "utilities/string_hash.h"

/**
 * @brief Owner of all animation data for a Scene::Entry. Plays and blends animations.
 */
class EntryAnimation
{
  public:
    explicit EntryAnimation(const fastgltf::Asset &asset);

    void setAnimation(Animation &animation);
    void setAnimation(std::string_view name);
    void setAnimation(std::size_t index);
    void setAnimationTransition(std::string_view name, float blendDuration);

    void updateAnimation(std::vector<Primitive> &primitives, float deltaTime);

    std::size_t getNumJoints() const;
    std::size_t getNumAnimations() const;
    float getAnimationLength(std::size_t index) const;
    float getAnimationLength(std::string_view name) const;
    const std::vector<glm::mat4> &getJointMatrices() const;

  private:
    Animator animator_;
    std::vector<Animation> animations_;
    std::optional<Skeleton> skeleton_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> animationMap_;

    std::size_t findAnimation(std::string_view name) const;
};
