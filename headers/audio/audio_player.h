#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/sound_effect.h"
#include "models/aabb.h"

class AudioPlayer
{
  public:
    explicit AudioPlayer(const std::filesystem::path &jsonFile);

    void update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront);
    void playFootstepEffect();
    void playLandingEffect();
    void playJumpEffect();

  private:
    struct Trigger
    {
        AABB aabb;
        std::size_t soundIndex{0};
        bool enabled{false};
        bool oneShot{false};
        bool loop{false};
        bool hasPlayed{false};
    };

    struct SoundRange
    {
        std::size_t start{0};
        std::size_t count{0};
    };

    struct CharacterSounds
    {
        SoundRange footsteps;
        SoundRange landings;
        SoundRange jumps;
    };

    AudioEngine engine_;
    std::vector<Trigger> triggers_;
    CharacterSounds characterSounds_;
    std::vector<std::unique_ptr<SoundEffect>> soundEffects_;
};
