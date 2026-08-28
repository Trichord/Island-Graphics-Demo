#include "audio/sound_effect.h"

#include <algorithm>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <miniaudio.h>
#include <stdexcept>
#include <string>

#include "audio/audio_engine.h"
#include "utilities/random.h"

SoundEffect::SoundEffect(AudioEngine &engine, const SoundEffectEntry &entry, const glm::vec3 &position)
    : volumeLow_{entry.volumeLow}, volumeHigh_{entry.volumeHigh}, pitchLow_{entry.pitchLow}, pitchHigh_{entry.pitchHigh}
{
    if (entry.soundPath.empty())
        throw std::runtime_error("Empty filepath");

    if (volumeLow_ > volumeHigh_ || pitchLow_ > pitchHigh_)
    {
        std::cerr << "ERROR: min value greater than max value from Sound Effect: " << entry.soundPath << std::endl;
        throw std::runtime_error("Undefined sound effect range");
    }

    ma_uint32 flags{0};
    if (entry.streaming)
        flags |= MA_SOUND_FLAG_STREAM;
    else
        flags |= MA_SOUND_FLAG_DECODE;

    if (!entry.spatialization)
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    const std::string pathStr{entry.soundPath.string()};
    ma_result result{ma_sound_init_from_file(&engine.engine, pathStr.c_str(), flags, nullptr, nullptr, &sound_)};

    if (result != MA_SUCCESS)
    {
        std::cerr << "Failed to load sound from " << pathStr << ": " << ma_result_description(result) << std::endl;
        throw std::runtime_error("Fatal sound loading error");
    }

    initialized_ = true;
    varyVolume();
    ma_sound_set_position(&sound_, position.x, position.y, position.z);
    ma_sound_get_length_in_seconds(&sound_, &length_);
}

SoundEffect::~SoundEffect()
{
    if (initialized_)
        ma_sound_uninit(&sound_);
}

float SoundEffect::getLengthInSeconds() const
{
    return length_ / ma_sound_get_pitch(&sound_);
}

void SoundEffect::setLooping(bool loop)
{
    ma_sound_set_looping(&sound_, loop ? MA_TRUE : MA_FALSE);
}

void SoundEffect::setPitch(float pitch)
{
    constexpr float minPitch{0.001f};
    ma_sound_set_pitch(&sound_, std::max(pitch, minPitch));
}

void SoundEffect::setVolume(float volume)
{
    currentVolume_ = volume;
    ma_sound_set_volume(&sound_, std::max(0.0f, currentVolume_ * volumeMultiplier_));
}

void SoundEffect::setDistanceFade(float factor)
{
    volumeMultiplier_ = std::clamp(factor, 0.0f, 1.0f);
    ma_sound_set_volume(&sound_, std::max(0.0f, currentVolume_ * volumeMultiplier_));
}

void SoundEffect::setPosition(const glm::vec3 &position)
{
    ma_sound_set_position(&sound_, position.x, position.y, position.z);
}

float SoundEffect::varyPitch()
{
    const float pitch{random::floatMS(pitchLow_, pitchHigh_)};
    setPitch(pitch);

    return pitch;
}

float SoundEffect::varyVolume()
{
    const float volume{random::floatMS(volumeLow_, volumeHigh_)};
    setVolume(volume);

    return volume;
}

ma_result SoundEffect::play(bool reset)
{
    if (reset)
        ma_sound_seek_to_pcm_frame(&sound_, 0);

    return ma_sound_start(&sound_);
}