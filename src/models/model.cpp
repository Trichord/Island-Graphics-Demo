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
#include "models/animator.h"
#include "models/primitive.h"
#include "utilities/asset_loader.h"
#include "utilities/texture_manager.h"

Model::Model(const std::filesystem::path &path)
    : asset_{AssetLoader::loadAsset(path)}, filePath_{path}, primitives_{AssetLoader::loadMeshes(asset_)},
      skeleton_{AssetLoader::loadSkeleton(asset_)}, animations_{AssetLoader::loadAnimations(asset_)}
{
    if (skeleton_.has_value() && skeleton_->skin.has_value())
        recalculateAnimatedAABBs();
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

const std::optional<std::vector<Animation>> &Model::getAnimations() const
{
    return animations_;
}

const std::optional<Skeleton> &Model::getSkeleton() const
{
    return skeleton_;
}

void Model::recalculateAnimatedAABBs()
{
    constexpr float timeStep{0.1f};
    
    const std::size_t numJoints{skeleton_->skin.has_value() ? skeleton_->skin->invBindMatrices.size() : 0};
    Animator animator{skeleton_->parents.size(), numJoints};

    for (Animation &animation : animations_.value())
    {
        for (float dt{0.0f}; dt <= animation.end; dt += timeStep)
        {
            animator.play(animation, skeleton_.value(), primitives_, dt);

            const std::vector<glm::mat4> &jointMatrices{animator.getJointMatrices()};
            
            for (Primitive &primitive : primitives_)
            {
                if (!primitive.vertexSkins.has_value())
                    continue;

                glm::vec3 min{primitive.aabb.getMin()};
                glm::vec3 max{primitive.aabb.getMax()};

                const AABB animatedAABB{primitive.aabb.transform(primitive.modelSpaceTransform)};
                const std::vector<Primitive::VertexSkin>& skin{primitive.vertexSkins.value()};

                for (std::size_t i{0}; i < primitive.vertices.size(); ++i)
                {
                    const glm::vec3 &position{primitive.vertices[i].position};
                    const glm::uvec4 &boneIDs{skin[i].boneIDs};
                    const glm::vec4 &weights{skin[i].weights};
                
                    glm::vec4 acc{0.0f};

                    for (std::size_t i{0}; i < 4; ++i)
                    {
                        if (weights[i] > 0.0f)
                            acc += weights[i] * jointMatrices[boneIDs[i]] * glm::vec4(position, 1.0f);
                    }

                    min = glm::min(min, glm::vec3{acc});
                    max = glm::max(max, glm::vec3{acc});
                }

                primitive.aabb = {min, max};
            }
        }
    }
}