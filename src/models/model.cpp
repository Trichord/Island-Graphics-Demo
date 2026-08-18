#include "models/model.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/animation/animator.h"
#include "models/animation/skeletal_animation.h"
#include "models/primitive.h"
#include "renderer/shader/graphics_shader.h"
#include "utilities/asset_loader.h"
#include "utilities/texture_manager.h"

Model::Model(const std::filesystem::path &path, ShaderType shader)
    : asset_{loader::loadAsset(path)}, filePath_{path}, primitives_{loader::loadMeshes(asset_)},
      skeletalAnimation_{loader::loadSkeletalAnimation(asset_)}
{
    if (skeletalAnimation_ && skeletalAnimation_->skeleton.skin)
        recalculateAnimatedAABBs();

    for (Primitive &primitive : primitives_)
        primitive.shaderType = shader;
}

const std::vector<Primitive> &Model::getPrimitives() const
{
    return primitives_;
}

const fastgltf::Asset &Model::getAsset() const
{
    return asset_;
}

const std::filesystem::path &Model::getPath() const
{
    return filePath_;
}

const std::optional<SkeletalAnimation> &Model::getSkeletalAnimation() const
{
    return skeletalAnimation_;
}

void Model::recalculateAnimatedAABBs()
{
    constexpr float timeStep{0.1f};

    const auto &skin = skeletalAnimation_->skeleton.skin;
    std::vector<Animation> &animations{skeletalAnimation_->animations};

    const std::size_t numJoints{skin ? skin->invBindMatrices.size() : 0};
    const std::size_t numNodes{skeletalAnimation_->skeleton.parents.size()};
    Animator animator{numNodes, numJoints};

    for (Animation &animation : animations)
    {
        animator.setAnimation(animation);

        for (float dt{0.0f}; dt <= animation.end; dt += timeStep)
        {
            animator.update(skeletalAnimation_->skeleton, primitives_, dt);

            const std::vector<glm::mat4> &jointMatrices{animator.getJointMatrices()};

            for (Primitive &primitive : primitives_)
            {
                if (!primitive.vertexSkins)
                    continue;

                glm::vec3 min{primitive.aabb.getMin()};
                glm::vec3 max{primitive.aabb.getMax()};

                const AABB animatedAABB{primitive.aabb.transform(primitive.modelSpaceTransform)};
                const std::vector<Primitive::VertexSkin> &vertexSkins{primitive.vertexSkins.value()};

                for (std::size_t i{0}; i < primitive.vertices.size(); ++i)
                {
                    const glm::vec3 &position{primitive.vertices[i].position};
                    const glm::uvec4 &boneIDs{vertexSkins[i].boneIDs};
                    const glm::vec4 &weights{vertexSkins[i].weights};

                    glm::vec4 acc{0.0f};

                    for (glm::length_t j{0}; j < 4; ++j)
                    {
                        if (weights[j] > 0.0f)
                            acc += weights[j] * jointMatrices[boneIDs[j]] * glm::vec4(position, 1.0f);
                    }

                    min = glm::min(min, glm::vec3{acc});
                    max = glm::max(max, glm::vec3{acc});
                }

                primitive.aabb = {min, max};
            }
        }
    }
}