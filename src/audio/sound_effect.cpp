#include "audio/sound_effect.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <miniaudio.h>
#include <stdexcept>
#include <string>

#include "audio/audio_engine.h"

SoundEffect::SoundEffect(AudioEngine &engine, const std::filesystem::path &path, float volume, ma_uint32 flags,
                         const glm::vec3 &position)
    : initialVolume_{volume}
{
    if (path.empty())
        throw std::runtime_error("Empty filepath");

    const std::string pathStr{path.string()};
    ma_result result{ma_sound_init_from_file(&engine.engine, pathStr.c_str(), flags, nullptr, nullptr, &sound_)};

    if (result != MA_SUCCESS)
        throw std::runtime_error("Failed to load sound from " + pathStr + ": " + ma_result_description(result));

    initialized_ = true;
    ma_sound_set_volume(&sound_, volume);
    ma_sound_set_position(&sound_, position.x, position.y, position.z);
}

SoundEffect::~SoundEffect()
{
    if (initialized_)
        ma_sound_uninit(&sound_);
}

void SoundEffect::setLooping(bool loop)
{
    ma_sound_set_looping(&sound_, loop ? MA_TRUE : MA_FALSE);
}

float SoundEffect::getInitialVolume() const
{
    return initialVolume_;
}

ma_result SoundEffect::play(float volume, float pitch, bool reset)
{
    ma_sound_set_volume(&sound_, volume);
    ma_sound_set_pitch(&sound_, pitch);

    if (reset)
        ma_sound_seek_to_pcm_frame(&sound_, 0);

    return ma_sound_start(&sound_);
}