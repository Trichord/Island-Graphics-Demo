#include "audio/audio_player.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <ios>
#include <iostream>
#include <memory>

#include <miniaudio.h>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

#include "audio/audio_file.h"
#include "audio/sound_effect.h"
#include "utilities/world_vectors.h"

namespace
{
std::minstd_rand &rnd()
{
    static std::minstd_rand gen{std::random_device{}()};
    return gen;
}

std::size_t randomIndex(std::size_t size)
{
    assert(size > 0);
    return std::uniform_int_distribution<size_t>(std::size_t{0}, std::size_t{size - 1})(rnd());
}

float randomFloat(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(rnd());
}
} // namespace

AudioPlayer::AudioPlayer(const std::filesystem::path &jsonFile)
{
    using namespace nlohmann;

    if (jsonFile.empty() || !std::filesystem::exists(jsonFile))
    {
        std::cerr << "ERROR: Could not load scene " << jsonFile << std::endl;
        throw std::runtime_error("Invalid Scene");
    }

    std::ifstream file{jsonFile};

    if (!file.is_open())
    {
        std::cerr << "ERROR: Failed to open " << jsonFile << std::endl;
        throw std::runtime_error("Failed to open file");
    }

    if (file.peek() == std::ifstream::traits_type::eof())
        return;

    AudioFile audio;

    try
    {
        audio = json::parse(file).get<AudioFile>();
    }

    catch (const json::exception &e)
    {
        std::cerr << e.what() << std::endl;
        throw;
    }

    // Initialize the audio engine now that the json has been loaded

    ma_engine_listener_set_world_up(&engine_.engine, 0, WorldVectors::worldUp.x, WorldVectors::worldUp.y,
                                    WorldVectors::worldUp.z);

    // Reserve the max of footsteps and 2, since a landing event plays 2 simultaneous footsteps
    const std::size_t landingsSize{std::max(audio.characterSounds.footsteps.size(), std::size_t{2})};

    triggers_.reserve(audio.audioTriggers.size());
    soundEffects_.reserve(audio.audioTriggers.size() + audio.characterSounds.footsteps.size() +
                          audio.characterSounds.jumps.size() + landingsSize);

    for (const AudioTriggerEntry &entry : audio.audioTriggers)
    {
        if (entry.soundEffectEntry.soundPath.empty())
            continue;

        if (!std::filesystem::exists(entry.soundEffectEntry.soundPath))
            throw std::runtime_error("Failed to load sound from " + entry.soundEffectEntry.soundPath.string());

        ma_uint32 flags{0};
        if (entry.soundEffectEntry.streaming)
            flags |= MA_SOUND_FLAG_STREAM;
        else
            flags |= MA_SOUND_FLAG_DECODE;

        if (!entry.soundEffectEntry.spatialization)
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        triggers_.emplace_back(Trigger{
            .aabb = entry.triggerZone,
            .soundIndex = soundEffects_.size(),
            .oneShot = entry.playOnce,
            .loop = entry.loop,
        });

        soundEffects_.emplace_back(std::make_unique<SoundEffect>(engine_, entry.soundEffectEntry.soundPath,
                                                                 entry.soundEffectEntry.volume, flags, entry.position));
    }

    constexpr ma_uint32 characterFlags{MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION};
    characterSounds_.footsteps.start = soundEffects_.size();
    characterSounds_.footsteps.count = audio.characterSounds.footsteps.size();
    characterSounds_.landings.start = characterSounds_.footsteps.start;
    characterSounds_.landings.count = landingsSize;

    for (const SoundEffectEntry &footstep : audio.characterSounds.footsteps)
        soundEffects_.emplace_back(
            std::make_unique<SoundEffect>(engine_, footstep.soundPath, footstep.volume, characterFlags));

    // Landings need two footsteps to play simultaneously but only one footstep exists
    if (characterSounds_.footsteps.count == 1)
    {
        SoundEffectEntry &footstep{audio.characterSounds.footsteps.front()};
        soundEffects_.emplace_back(
            std::make_unique<SoundEffect>(engine_, footstep.soundPath, footstep.volume, characterFlags));
    }

    characterSounds_.jumps.start = soundEffects_.size();
    characterSounds_.jumps.count = audio.characterSounds.jumps.size();

    for (const SoundEffectEntry &jump : audio.characterSounds.jumps)
        soundEffects_.emplace_back(std::make_unique<SoundEffect>(engine_, jump.soundPath, jump.volume, characterFlags));
}

void AudioPlayer::update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront)
{
    if (triggers_.empty())
        return;

    ma_engine_listener_set_position(&engine_.engine, 0, listenerPos.x, listenerPos.y, listenerPos.z);
    ma_engine_listener_set_direction(&engine_.engine, 0, listenerFront.x, listenerFront.y, listenerFront.z);

    for (std::size_t i{0}; i < triggers_.size(); ++i)
    {
        Trigger &trigger{triggers_[i]};

        if (trigger.oneShot && trigger.hasPlayed)
            continue;

        const bool wasEnabled{trigger.enabled};
        trigger.enabled = trigger.aabb.pointInside(listenerPos);

        if (wasEnabled == trigger.enabled)
            continue;

        SoundEffect &effect{*soundEffects_[trigger.soundIndex]};

        if (trigger.enabled)
        {
            effect.setLooping(trigger.loop);

            if (effect.play(effect.getInitialVolume()) == MA_SUCCESS)
                trigger.hasPlayed = true;
        }
        else
            effect.setLooping(false);
    }
}

void AudioPlayer::playFootstepEffect()
{
    constexpr float pitchVariation{0.2f};
    constexpr float volumeVariation{0.3f};

    if (!characterSounds_.footsteps.count)
        return;

    const std::size_t index{randomIndex(characterSounds_.footsteps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.footsteps.start + index]};

    // Only make footsteps randomly softer. Normal footstep volume is already plenty.
    const float initialVolume{effect.getInitialVolume()};
    const float volume{randomFloat(initialVolume - volumeVariation, initialVolume)};
    const float pitch{randomFloat(1.0f - pitchVariation, 1.0f + pitchVariation)};

    effect.play(volume, pitch, true);
}

void AudioPlayer::playLandingEffect()
{
    constexpr float landingPitch{0.7f};
    constexpr float pitchVariation{0.15f};
    constexpr float volumeLow{0.2f};
    constexpr float volumeHigh{0.4f};

    if (!characterSounds_.footsteps.count)
        return;

    const std::size_t index{randomIndex(characterSounds_.landings.count)};
    std::size_t otherIndex{randomIndex(characterSounds_.landings.count - 1)};

    if (otherIndex >= index)
        ++otherIndex;

    SoundEffect &first{*soundEffects_[characterSounds_.landings.start + index]};
    SoundEffect &second{*soundEffects_[characterSounds_.landings.start + otherIndex]};

    // Landing sounds should be lower pitched and louder
    const float firstInitialVolume{first.getInitialVolume()};
    const float firstVolume{randomFloat(firstInitialVolume + volumeLow, firstInitialVolume + volumeHigh)};
    const float firstPitch{randomFloat(landingPitch - pitchVariation, landingPitch + pitchVariation)};

    const float secondInitialVolume{second.getInitialVolume()};
    const float secondVolume{randomFloat(secondInitialVolume + volumeLow, secondInitialVolume + volumeHigh)};
    const float secondPitch{randomFloat(landingPitch - pitchVariation, landingPitch + pitchVariation)};

    first.play(firstVolume, firstPitch, true);
    second.play(secondVolume, secondPitch, true);
}

void AudioPlayer::playJumpEffect()
{
    constexpr float pitchVariation{0.2f};
    constexpr float volumeVariation{0.01f};

    if (!characterSounds_.jumps.count)
        return;

    const std::size_t index{randomIndex(characterSounds_.jumps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.jumps.start + index]};

    const float initialVolume{effect.getInitialVolume()};
    const float volume{randomFloat(initialVolume - volumeVariation, initialVolume + volumeVariation)};
    const float pitch{randomFloat(1.0f - pitchVariation, 1.0f + pitchVariation)};

    playFootstepEffect();
    effect.play(volume, pitch, true);
}