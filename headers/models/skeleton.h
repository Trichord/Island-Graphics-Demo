#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

struct Skeleton
{
    // gLTF required vectors
    std::vector<int> parents;
    std::vector<glm::mat4> invBindMatrices;
    std::vector<glm::vec3> localTranslations;
    std::vector<glm::quat> localRotations;
    std::vector<glm::vec3> localScales;

    // Stashed data for the vertex
    std::vector<glm::vec4> weights;
    std::vector<glm::uvec4> boneIDs;

    // Recalculate each frame from animation changes
    std::vector<glm::mat4> globalTransforms;

    // Pass to shaders
    std::vector<glm::mat4> jointMatrices;

    Skeleton() = default;
    Skeleton(std::size_t numNodes, std::size_t numJoints, std::size_t numVertices)
    {
        parents.resize(numNodes, -1);
        localTranslations.resize(numNodes);
        localRotations.resize(numNodes);
        localScales.resize(numNodes);
        globalTransforms.resize(numNodes);

        invBindMatrices.resize(numJoints, glm::mat4{1.0f});
        jointMatrices.resize(numJoints);

        weights.resize(numVertices);
        boneIDs.resize(numVertices);
    }
};