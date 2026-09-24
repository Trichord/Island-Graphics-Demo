#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "models/aabb.h"

struct SoundEffectEntry
{
    std::filesystem::path soundPath;

    float volumeLow{1.0f};
    float volumeHigh{1.0f};
    float pitchLow{1.0f};
    float pitchHigh{1.0f};

    bool streaming{false};
    bool spatialization{false};
};

struct AudioTriggerEntry
{
    SoundEffectEntry soundEffectEntry;
    AABB triggerZone;
    glm::vec3 position{0.0f};
    std::string name{};

    bool loop{false};
    bool playOnce{false};
    float repeatDelay{0.0f};
    float fadeDistance{0.0f};
};

struct CharacterSoundEntry
{
    // Pitch modifier applied to all footstep sounds to create a different sounding landing effect (typically pitched down).
    float landingPitchScale{1.0f};

    // Volume modifier applied to all footstep sounds to create a (usually louder) landing effect.
    float landingVolumeScale{1.0f};

    std::vector<SoundEffectEntry> footsteps;
    std::vector<SoundEffectEntry> jumps;
};

struct CollisionSoundEntry
{
    SoundEffectEntry soundEffectEntry;
    std::string name{};
    float repeatDelay{0.0f};
    float minSpeed{0.0f};
};

struct AudioFile
{
    std::vector<CollisionSoundEntry> collisionSounds;
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

    sfx.volumeLow = j.value("volumeLow", 1.0f);
    sfx.volumeHigh = j.value("volumeHigh", 1.0f);
    sfx.pitchLow = j.value("pitchLow", 1.0f);
    sfx.pitchHigh = j.value("pitchHigh", 1.0f);

    sfx.streaming = j.value("streaming", false);
    sfx.spatialization = j.value("spatialization", false);
}

inline void from_json(const nlohmann::json &j, AudioTriggerEntry &entry)
{
    glm::vec3 center{j.contains("triggerCenter") ? parseVec3(j.at("triggerCenter")) : glm::vec3{0.0f}};
    glm::vec3 extent{j.contains("triggerExtent") ? parseVec3(j.at("triggerExtent")) : glm::vec3{0.0f}};
    entry.triggerZone = {center - extent, center + extent};
   
    entry.soundEffectEntry = j.value("soundEffect", SoundEffectEntry{});
    entry.position = j.contains("position") ? parseVec3(j.at("position")) : center;
    entry.name = j.value("name", std::string{});
    entry.loop = j.value("loop", false);
    entry.playOnce = j.value("playOnce", false);
    entry.repeatDelay = j.value("repeatDelay", 0.0f);
    entry.fadeDistance = j.value("fadeDistance", 0.0f);
}

inline void from_json(const nlohmann::json &j, CharacterSoundEntry &character)
{
    character.landingPitchScale = j.value("landingPitchScale", 1.0f);
    character.landingVolumeScale = j.value("landingVolumeScale", 1.0f);

    character.footsteps = j.value("footsteps", std::vector<SoundEffectEntry>{});
    character.jumps = j.value("jumps", std::vector<SoundEffectEntry>{});
}

inline void from_json(const nlohmann::json &j, CollisionSoundEntry &collision)
{
    collision.soundEffectEntry = j.value("soundEffect", SoundEffectEntry{});
    collision.name = j.value("name", std::string{});
    collision.minSpeed = j.value("minSpeed", 0.0f);
    collision.repeatDelay = j.value("repeatDelay", 0.0f);
}

inline void from_json(const nlohmann::json &j, AudioFile &audioFile)
{
    audioFile.collisionSounds = j.value("collisions", std::vector<CollisionSoundEntry>{});
    audioFile.audioTriggers = j.value("triggers", std::vector<AudioTriggerEntry>{});
    audioFile.characterSounds = j.value("character", CharacterSoundEntry{});
}