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
    struct Zone
    {
        AABB zone;
        ma_sound sound;
        bool enabled{false};
        bool oneShot{false};
    };

    ma_engine audioEngine_ = {};
    std::vector<Zone> zones_;
    std::vector<ma_sound> footsteps_;
    std::vector<bool> hasPlayed_;

    void playSounds() const;
};
