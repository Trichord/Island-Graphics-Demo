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
    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;
    AudioPlayer(AudioPlayer &&) = delete;
    AudioPlayer &operator=(AudioPlayer &&) = delete;
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
    std::vector<Trigger> triggers_;
    std::vector<SoundEffect> footsteps_;
    std::vector<SoundEffect> landings_;
    std::vector<bool> hasPlayed_;

    void playSound(ma_sound &sound, float volume, float pitch);
};
