#include "audio/audio_player.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <miniaudio.h>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

#include "audio/audio_file.h"
#include "audio/sound_effect.h"
#include "scene/scene.h"
#include "utilities/world_vectors.h"
#include "utilities/random.h"


AudioPlayer::AudioPlayer(const Scene &scene, const std::filesystem::path &audioJson)
{
    using namespace nlohmann;

    if (audioJson.empty() || !std::filesystem::exists(audioJson))
    {
        std::cerr << "ERROR: Could not load scene " << audioJson << std::endl;
        throw std::runtime_error("Invalid Scene");
    }

    std::ifstream file{audioJson};

    if (!file.is_open())
    {
        std::cerr << "ERROR: Failed to open " << audioJson << std::endl;
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

    ma_engine_listener_set_world_up(&engine_.engine, 0, WorldVectors::worldUp.x, WorldVectors::worldUp.y,
                                    WorldVectors::worldUp.z);

    // Reserve one extra footstep if size is one, since landings require one extra footstep sound
    const std::size_t footstepsSize{audio.characterSounds.footsteps.size()};
    const std::size_t totalFootsteps{footstepsSize + (footstepsSize == 1 ? 1 : 0)};

    triggers_.reserve(audio.audioTriggers.size());
    soundEffects_.reserve(audio.audioTriggers.size() + audio.characterSounds.jumps.size() + totalFootsteps);

    for (const AudioTriggerEntry &triggerEntry : audio.audioTriggers)
    {
        if (triggerEntry.soundEffectEntry.soundPath.empty())
            continue;

        if (!std::filesystem::exists(triggerEntry.soundEffectEntry.soundPath))
            throw std::runtime_error("Failed to load sound from " + triggerEntry.soundEffectEntry.soundPath.string());

        ma_uint32 flags{0};
        if (triggerEntry.soundEffectEntry.streaming)
            flags |= MA_SOUND_FLAG_STREAM;
        else
            flags |= MA_SOUND_FLAG_DECODE;

        if (!triggerEntry.soundEffectEntry.spatialization)
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        // Resolve audio name -> scene name -> handle, for dirty handle updates
        if (!triggerEntry.name.empty())
        {
            const Scene::Entry &sceneEntry{scene.viewEntry(triggerEntry.name)};
            triggersByHandle_.emplace(sceneEntry.handle, triggers_.size());
        }

        AABB outer{triggerEntry.triggerZone};
        outer.setHalfExtent(outer.getHalfExtent() + triggerEntry.fadeDistance);

        triggers_.emplace_back(Trigger{
            .innerAABB = triggerEntry.triggerZone,
            .outerAABB = outer,
            .soundIndex = soundEffects_.size(),
            .oneShot = triggerEntry.playOnce,
            .loop = triggerEntry.loop,
            .repeatDelay = triggerEntry.repeatDelay,
            .fadeDistance = triggerEntry.fadeDistance,
        });

        soundEffects_.emplace_back(std::make_unique<SoundEffect>(engine_, triggerEntry.soundEffectEntry.soundPath,
                                                                 triggerEntry.soundEffectEntry.volume, flags,
                                                                 triggerEntry.position));
    }

    constexpr ma_uint32 characterFlags{MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION};
    characterSounds_.footsteps.start = soundEffects_.size();
    characterSounds_.footsteps.count = totalFootsteps;

    for (const SoundEffectEntry &footstep : audio.characterSounds.footsteps)
        soundEffects_.emplace_back(
            std::make_unique<SoundEffect>(engine_, footstep.soundPath, footstep.volume, characterFlags));

    // Landings need two footsteps to play simultaneously but only one footstep exists
    if (footstepsSize == 1)
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

void AudioPlayer::updateTriggers(UpdatedEntriesView updatedEntries)
{
    for (const Scene::Entry &entry : updatedEntries)
    {
        auto handleIter = triggersByHandle_.find(entry.handle);

        // Many scene objects will be dirty and not have any audio
        if (handleIter == triggersByHandle_.end())
            continue;

        if (entry.worldTransforms.size() != 1)
            throw std::logic_error("Cannot update instanced audio");

        Trigger &trigger{triggers_[handleIter->second]};
        trigger.innerAABB.setCenter(entry.worldTransforms.front().getPosition());
        trigger.outerAABB.setCenter(trigger.innerAABB.getCenter());
    }
}

void AudioPlayer::playTriggers(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront, float deltaTime)
{
    ma_engine_listener_set_position(&engine_.engine, 0, listenerPos.x, listenerPos.y, listenerPos.z);
    ma_engine_listener_set_direction(&engine_.engine, 0, listenerFront.x, listenerFront.y, listenerFront.z);

    for (Trigger &trigger : triggers_)
    {
        trigger.timeAccumulator += deltaTime;

        // Must be first to keep hasPlayed from being reset and one-shots as single playbacks
        if (trigger.oneShot && trigger.hasPlayed)
            continue;

        trigger.enabled = trigger.innerAABB.pointInside(listenerPos);

        SoundEffect &effect{*soundEffects_[trigger.soundIndex]};
        const float volumeMultiplier{trigger.getVolumeMultiplier(listenerPos)};
        effect.setVolume(effect.getInitialVolume() * volumeMultiplier);

        // Not audible
        if (trigger.fadeDistance > 0.0f ? volumeMultiplier <= 0.0f : !trigger.enabled)
        {
            trigger.hasPlayed = false;
            trigger.timeAccumulator = 0.0f;
            effect.setLooping(false);

            continue;
        }

        const bool delayedRepeat{trigger.loop && trigger.repeatDelay > 0.0f};

        if (trigger.hasPlayed)
        {
            if (!delayedRepeat)
                continue;

            if (trigger.timeAccumulator < effect.getLengthInSeconds() + trigger.repeatDelay)
                continue;
        }

        trigger.timeAccumulator = 0.0f;
        effect.setLooping(trigger.loop && !delayedRepeat);

        if (effect.play() == MA_SUCCESS)
            trigger.hasPlayed = true;
    }
}

void AudioPlayer::playFootstepEffect()
{
    constexpr float pitchVariation{0.2f};
    constexpr float volumeVariation{0.3f};

    if (!characterSounds_.footsteps.count)
        return;

    const std::size_t index{random::index(characterSounds_.footsteps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.footsteps.start + index]};

    // Only make footsteps randomly softer. Normal footstep volume is already plenty.
    const float initialVolume{effect.getInitialVolume()};
    effect.setVolume(random::floatMS(initialVolume - volumeVariation, initialVolume));
    effect.setPitch(random::floatMS(1.0f - pitchVariation, 1.0f + pitchVariation));

    effect.play(true);
}

void AudioPlayer::playLandingEffect()
{
    constexpr float landingPitch{0.7f};
    constexpr float pitchVariation{0.15f};
    constexpr float volumeLow{0.2f};
    constexpr float volumeHigh{0.4f};

    if (characterSounds_.footsteps.count < 2)
        return;

    const std::size_t index{random::index(characterSounds_.footsteps.count)};
    std::size_t otherIndex{random::index(characterSounds_.footsteps.count - 1)};

    if (otherIndex >= index)
        ++otherIndex;

    SoundEffect &first{*soundEffects_[characterSounds_.footsteps.start + index]};
    SoundEffect &second{*soundEffects_[characterSounds_.footsteps.start + otherIndex]};

    // Landing sounds should be lower pitched and louder
    const float firstInitialVolume{first.getInitialVolume()};
    first.setVolume(random::floatMS(firstInitialVolume + volumeLow, firstInitialVolume + volumeHigh));
    first.setPitch(random::floatMS(landingPitch - pitchVariation, landingPitch + pitchVariation));

    const float secondInitialVolume{second.getInitialVolume()};
    second.setVolume(random::floatMS(secondInitialVolume + volumeLow, secondInitialVolume + volumeHigh));
    second.setPitch(random::floatMS(landingPitch - pitchVariation, landingPitch + pitchVariation));

    first.play(true);
    second.play(true);
}

void AudioPlayer::playJumpEffect()
{
    constexpr float pitchVariation{0.2f};
    constexpr float volumeVariation{0.01f};

    if (!characterSounds_.jumps.count)
        return;

    const std::size_t index{random::index(characterSounds_.jumps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.jumps.start + index]};

    const float initialVolume{effect.getInitialVolume()};
    effect.setVolume(random::floatMS(initialVolume - volumeVariation, initialVolume + volumeVariation));
    effect.setPitch(random::floatMS(1.0f - pitchVariation, 1.0f + pitchVariation));

    playFootstepEffect();
    effect.play(true);
}