#include "scene/entry_animation.h"

#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "models/animation/animation.h"
#include "models/animation/node_graph.h"
#include "models/primitive.h"
#include "utilities/asset_loader.h"

EntryAnimation::EntryAnimation(const fastgltf::Asset &asset)
    : animations_{loader::loadAnimations(asset)}, skeleton_{loader::loadSkeleton(asset)}
{
    NodeGraph graph{loader::loadNodeGraph(asset)};
    loader::reorderGraph(graph, animations_, skeleton_);

    for (std::size_t i{0}; i < animations_.size(); ++i)
        if (!animations_[i].name.empty())
            animationMap_.emplace(animations_[i].name, i);

    animator_ = {std::move(graph), skeleton_ ? skeleton_->invBindMatrices.size() : 0};
}

void EntryAnimation::setAnimation(Animation &animation)
{
    animator_.setAnimation(animation);
}

void EntryAnimation::setAnimation(std::string_view name)
{
    animator_.setAnimation(animations_[findAnimation(name)]);
}

void EntryAnimation::setAnimation(std::size_t index)
{
    if (index >= animations_.size())
    {
        std::cerr << "Error: index " << index << " out of bounds\n";
        throw std::runtime_error("Animation not found");
    }

    animator_.setAnimation(animations_[index]);
}

void EntryAnimation::setAnimationTransition(std::string_view name, float blendDuration)
{
    animator_.setTransition(animations_[findAnimation(name)], blendDuration);
}

void EntryAnimation::updateAnimation(std::vector<Primitive> &primitives, float deltaTime)
{
    animator_.update(primitives, skeleton_, deltaTime);
}

std::size_t EntryAnimation::findAnimation(std::string_view name) const
{
    auto iter = animationMap_.find(name);

    if (iter == animationMap_.end())
    {
        std::cerr << "Error: animation " << name << " not found\n";
        throw std::runtime_error("Animation not found");
    }

    return iter->second;
}

std::size_t EntryAnimation::getNumJoints() const
{
    if (!skeleton_)
        return 0;

    return skeleton_->invBindMatrices.size();
}

std::size_t EntryAnimation::getNumAnimations() const
{
    return animations_.size();
}

float EntryAnimation::getAnimationLength(std::size_t index) const
{
    if (index >= animations_.size())
    {
        std::cerr << "Error: index " << index << " out of bounds\n";
        throw std::runtime_error("Animation not found");
    }

    const Animation &animation{animations_[index]};
    return animation.end;
}

float EntryAnimation::getAnimationLength(std::string_view name) const
{
    const Animation &animation{animations_[findAnimation(name)]};
    return animation.end;
}

const std::vector<glm::mat4> &EntryAnimation::getJointMatrices() const
{
    return animator_.getJointMatrices();
}