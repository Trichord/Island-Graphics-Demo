#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <vector>

#include "models/aabb.h"

struct AudioEntry
{
    std::filesystem::path soundPath;
    AABB zone;
    bool loop{false};
    bool oneShot{false};
    bool streaming{false};
    bool playImmediate{false};
    bool spatialization{false};
};

struct AudioFile
{
    std::vector<AudioEntry> audioEntries;
    std::vector<std::filesystem::path> footsteps;
};

inline glm::vec3 parseVec3(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

inline void from_json(const nlohmann::json &j, AudioEntry &entry)
{
    entry.soundPath = j.value("soundPath", std::filesystem::path{});

    glm::vec3 center{j.contains("zoneCenter") ? parseVec3(j.at("zoneCenter")) : glm::vec3{0.0f}};
    glm::vec3 extent{j.contains("zoneExtent") ? parseVec3(j.at("zoneExtent")) : glm::vec3{0.0f}};
    entry.zone = {center - extent, center + extent};

    entry.loop = j.value("loop", false);
    entry.oneShot = j.value("oneShot", false);
    entry.streaming = j.value("streaming", false);
    entry.playImmediate = j.value("playImmediate", false);
    entry.spatialization = j.value("spatialization", true);
}

inline void from_json(const nlohmann::json &j, AudioFile &audioFile)
{
    audioFile.audioEntries = j.value("entries", std::vector<AudioEntry>{});
    audioFile.footsteps = j.value("footsteps", std::vector<std::filesystem::path>{});
}