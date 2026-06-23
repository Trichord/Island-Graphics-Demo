#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <vector>
#include <unordered_map>

struct Skeleton
{
    struct Skin
    {
        std::vector<std::size_t> jointNodes;
        std::vector<glm::mat4> invBindMatrices;

        Skin() = default;
        Skin(std::size_t numJoints)
        {
            invBindMatrices.resize(numJoints, glm::mat4{1.0f});
        }
    };

    std::vector<int> parents;
    std::unordered_map<std::size_t, std::vector<size_t>> nodeToPrimitive;

    std::vector<glm::vec3> restTranslations;
    std::vector<glm::quat> restRotations;
    std::vector<glm::vec3> restScales;

    std::optional<Skin> skin;

    Skeleton() = default;

    Skeleton(std::size_t numNodes)
    {
        parents.resize(numNodes, -1);

        restTranslations.resize(numNodes);
        restRotations.resize(numNodes);
        restScales.resize(numNodes);
    }

    Skeleton(std::size_t numNodes, std::size_t numJoints) : Skeleton(numNodes)
    {
        skin = {numJoints};       
    }
};