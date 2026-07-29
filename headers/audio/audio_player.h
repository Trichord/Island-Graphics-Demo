#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <miniaudio.h>
#include <vector>

#include "models/aabb.h"

class AudioPlayer
{
  public:
    AudioPlayer() = default;
    AudioPlayer(const std::filesystem::path &jsonFile);
    ~AudioPlayer();

    void update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront);
    void playFootstep();

  private:
    struct Trigger
    {
        AABB aabb;
        ma_sound sound;
        bool enabled{false};
        bool oneShot{false};
    };

    struct Footstep
    {
        // Store the volume locally so it can be randomly altered
        float volume{1.0f};
        ma_sound sound;
    };

    ma_engine audioEngine_ = {};
    std::vector<Trigger> trigger_;
    std::vector<Footstep> footsteps_;
    std::vector<bool> hasPlayed_;

    void playSounds() const;
};
