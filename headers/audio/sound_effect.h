#pragma once

#include <glm/glm.hpp>
#include <miniaudio.h>

#include "audio/audio_engine.h"
#include "audio/audio_file.h"

class SoundEffect
{
  public:
    static constexpr float defaultPitch{1.0f};

    SoundEffect() = default;
    SoundEffect(AudioEngine &engine, const SoundEffectEntry &entry, const glm::vec3 &position = glm::vec3{0.0f});
    ~SoundEffect();
    SoundEffect(const SoundEffect &) = delete;
    SoundEffect &operator=(const SoundEffect &) = delete;
    SoundEffect(SoundEffect &&) = delete;
    SoundEffect &operator=(SoundEffect &&) = delete;

    float getLengthInSeconds() const;

    void setLooping(bool loop);
    void setPitch(float pitch);
    void setVolume(float volume);
    void setDistanceFade(float factor);

    float varyPitch();
    float varyVolume();

    ma_result play(bool reset = false);

  private:
    ma_sound sound_{};
    float volumeLow_{1.0f};
    float volumeHigh_{1.0f};
    float pitchLow_{1.0f};
    float pitchHigh_{1.0f};
    float length_{0.0f};
    bool initialized_{false};
};