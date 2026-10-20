#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// Helpers for parsing JSON arrays into glm vectors
inline glm::vec3 parseVec3(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

inline glm::vec4 parseVec4(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
}
