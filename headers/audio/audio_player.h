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
    void playLanding();

  private:
    struct Trigger
    {
        AABB aabb;
        ma_sound sound;
        bool enabled{false};
        bool oneShot{false};
    };

    struct SoundEffect
    {
        // Store the volume locally so it can be randomly altered
        float volume{1.0f};
        ma_sound sound;
    };

    ma_engine audioEngine_ = {};
    std::vector<Trigger> trigger_;
    std::vector<SoundEffect> footsteps_;
    std::vector<SoundEffect> landings_;
    std::vector<bool> hasPlayed_;

    void playRandomSound(std::vector<SoundEffect> &sounds, float pitchLow, float pitchHigh, float volumeLow,
                         float volumeHigh);
};
