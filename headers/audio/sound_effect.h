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
    void setPosition(const glm::vec3 &position);

    /**
     * @brief Randomly generate and set pitch in the [low, high) range set at construction.
     *
     * @return The new pitch.
     */
    float varyPitch();

    /**
     * @brief Randomly generate and set volume in the [low, high) range set at construction.
     *
     * @return The new volume.
     */
    float varyVolume();

    /**
     * @brief Play the sound effect
     *
     * @param reset Force the sound effect to play from the beginning
     * 
     * @return MA_SUCCESS if the sound started or was already playing, otherwise the MiniAudio error code
     */
    ma_result play(bool reset = false);

  private:
    ma_sound sound_{};
    float volumeLow_{1.0f};
    float volumeHigh_{1.0f};
    float pitchLow_{1.0f};
    float pitchHigh_{1.0f};
    float length_{0.0f};
    float currentVolume_{0.0f};
    float volumeMultiplier_{1.0f};
    bool initialized_{false};
};