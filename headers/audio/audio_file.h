#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <vector>

#include "models/aabb.h"

struct SoundEffectEntry
{
    std::filesystem::path soundPath;

    float volume{1.0f};
    bool streaming{false};
    bool spatialization{false};
};

struct AudioTriggerEntry
{
    SoundEffectEntry soundEffectEntry;
    AABB triggerZone;
    glm::vec3 position{0.0f};
    bool loop{false};
    bool playOnce{false};
};

struct CharacterSoundEntry
{
    std::vector<SoundEffectEntry> footsteps;
    std::vector<SoundEffectEntry> jumps;
};

struct AudioFile
{
    std::vector<AudioTriggerEntry> audioTriggers;
    CharacterSoundEntry characterSounds;
};

inline glm::vec3 parseVec3(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

inline void from_json(const nlohmann::json &j, SoundEffectEntry &sfx)
{
    sfx.soundPath = j.value("soundPath", std::filesystem::path{});

    sfx.volume = j.value("volume", 1.0f);
    sfx.streaming = j.value("streaming", false);
    sfx.spatialization = j.value("spatialization", false);
}

inline void from_json(const nlohmann::json &j, AudioTriggerEntry &entry)
{
    entry.soundEffectEntry = j.value("soundEffect", SoundEffectEntry{});

    glm::vec3 center{j.contains("triggerCenter") ? parseVec3(j.at("triggerCenter")) : glm::vec3{0.0f}};
    glm::vec3 extent{j.contains("triggerExtent") ? parseVec3(j.at("triggerExtent")) : glm::vec3{0.0f}};
    entry.triggerZone = {center - extent, center + extent};
    entry.position = j.contains("position") ? parseVec3(j.at("position")) : center;

    entry.loop = j.value("loop", false);
    entry.playOnce = j.value("playOnce", false);
}

inline void from_json(const nlohmann::json &j, CharacterSoundEntry &character)
{
    character.footsteps = j.value("footsteps", std::vector<SoundEffectEntry>{});
    character.jumps = j.value("jumps", std::vector<SoundEffectEntry>{});
}

inline void from_json(const nlohmann::json &j, AudioFile &audioFile)
{
    audioFile.audioTriggers = j.value("triggers", std::vector<AudioTriggerEntry>{});
    audioFile.characterSounds = j.value("character", CharacterSoundEntry{});
}