#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

struct NodeGraph
{
    std::vector<int> parents;
    std::unordered_map<std::size_t, std::vector<size_t>> nodeToPrimitives;

    std::vector<glm::vec3> restTranslations;
    std::vector<glm::quat> restRotations;
    std::vector<glm::vec3> restScales;

    NodeGraph() = default;
    NodeGraph(std::size_t numNodes)
    {
        parents.resize(numNodes, -1);
        restTranslations.resize(numNodes);
        restRotations.resize(numNodes);
        restScales.resize(numNodes);
    }
};