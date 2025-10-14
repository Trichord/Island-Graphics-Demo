#pragma once

#include <glm/glm.hpp>
#include <vector>

static const inline glm::vec3 FLIGHT_HELMET_START =
    glm::vec3(0.0f, -2.3f, 50.0f);

static const inline glm::vec3 LIGHTBULB_START =
    glm::vec3(0.0f, 0.75f, 50.0f);

static const inline std::vector<glm::vec3> GRASS_POSITIONS = {
    FLIGHT_HELMET_START + glm::vec3(0.0f, 0.0f, -6.0f)};