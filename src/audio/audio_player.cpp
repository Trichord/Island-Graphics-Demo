#include "audio/audio_player.h"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <ios>
#include <iostream>
#include <memory>
#include <miniaudio.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/audio_file.h"
#include "audio/sound_effect.h"
#include "models/aabb.h"
#include "scene/scene.h"
#include "utilities/random.h"
#include "utilities/world_vectors.h"

AudioPlayer::AudioPlayer(const Scene &scene, const std::filesystem::path &audioJson)
{
    using namespace nlohmann;

    const auto addSound = [this](const SoundEffectEntry &entry, const glm::vec3 &pos = glm::vec3{0.0f}) {
        const std::size_t index{soundEffects_.size()};
        soundEffects_.emplace_back(std::make_unique<SoundEffect>(engine_, entry, pos));
        return index;
    };

    const auto addSoundRange = [this, &addSound](const std::vector<SoundEffectEntry> &entries) {
        const std::size_t start{soundEffects_.size()};
        for (const SoundEffectEntry &entry : entries)
            addSound(entry);
        return SoundRange{.start = start, .count = soundEffects_.size() - start};
    };

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
        json::parse(file).get_to(audio);
    }

    catch (const json::exception &e)
    {
        std::cerr << e.what() << std::endl;
        throw;
    }

    ma_engine_listener_set_world_up(&engine_.engine, 0, WorldVectors::worldUp.x, WorldVectors::worldUp.y,
                                    WorldVectors::worldUp.z);

    const std::vector<SoundEffectEntry> landings{createLandings(audio.characterSounds.footsteps,
                                                                audio.characterSounds.landingVolumeScale,
                                                                audio.characterSounds.landingPitchScale)};

    triggers_.reserve(audio.audioTriggers.size());
    soundEffects_.reserve(audio.audioTriggers.size() + audio.characterSounds.jumps.size() +
                          audio.characterSounds.footsteps.size() + landings.size());

    for (const AudioTriggerEntry &triggerEntry : audio.audioTriggers)
    {
        if (triggerEntry.soundEffectEntry.soundPath.empty())
            continue;

        if (!std::filesystem::exists(triggerEntry.soundEffectEntry.soundPath))
            throw std::runtime_error("Failed to load sound from " + triggerEntry.soundEffectEntry.soundPath.string());

        // Resolve audio name -> scene name -> handle, for dirty handle updates
        if (!triggerEntry.name.empty())
        {
            const Scene::Entry &sceneEntry{scene.viewEntry(triggerEntry.name)};
            triggersByHandle_.emplace(sceneEntry.handle, triggers_.size());
        }

        AABB outer{triggerEntry.triggerZone};
        outer.setHalfExtent(outer.getHalfExtent() + triggerEntry.fadeDistance);

        const std::size_t soundIndex{addSound(triggerEntry.soundEffectEntry, triggerEntry.position)};

        triggers_.emplace_back(Trigger{
            .innerAABB = triggerEntry.triggerZone,
            .outerAABB = outer,
            .soundIndex = soundIndex,
            .oneShot = triggerEntry.playOnce,
            .loop = triggerEntry.loop,
            .repeatDelay = triggerEntry.repeatDelay,
            .fadeDistance = triggerEntry.fadeDistance,
        });
    }

    characterSounds_.footsteps = addSoundRange(audio.characterSounds.footsteps);
    characterSounds_.landings = addSoundRange(landings);
    characterSounds_.jumps = addSoundRange(audio.characterSounds.jumps);
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

        trigger.enabled = trigger.outerAABB.pointInside(listenerPos);

        SoundEffect &effect{*soundEffects_[trigger.soundIndex]};
        const float fadeFactor{trigger.getFadeFactor(listenerPos)};

        // Not audible
        if (trigger.fadeDistance > 0.0f ? fadeFactor <= 0.0f : !trigger.enabled)
        {
            trigger.hasPlayed = false;
            trigger.timeAccumulator = 0.0f;
            effect.setLooping(false);

            continue;
        }
        effect.setDistanceFade(fadeFactor);
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
        effect.varyPitch();
        effect.varyVolume();

        if (effect.play() == MA_SUCCESS)
            trigger.hasPlayed = true;
    }
}

void AudioPlayer::playFootstepEffect()
{
    if (!characterSounds_.footsteps.count)
        return;

    const std::size_t index{random::index(characterSounds_.footsteps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.footsteps.start + index]};

    effect.varyVolume();
    effect.varyPitch();
    effect.play(true);
}

void AudioPlayer::playLandingEffect()
{
    if (characterSounds_.landings.count < 2)
        return;

    const std::size_t index{random::index(characterSounds_.landings.count)};
    std::size_t otherIndex{random::index(characterSounds_.landings.count - 1)};

    if (otherIndex >= index)
        ++otherIndex;

    SoundEffect &first{*soundEffects_[characterSounds_.landings.start + index]};
    SoundEffect &second{*soundEffects_[characterSounds_.landings.start + otherIndex]};

    first.varyVolume();
    first.varyPitch();
    second.varyVolume();
    second.varyPitch();

    first.play(true);
    second.play(true);
}

void AudioPlayer::playJumpEffect()
{
    if (!characterSounds_.jumps.count)
        return;

    const std::size_t index{random::index(characterSounds_.jumps.count)};
    SoundEffect &effect{*soundEffects_[characterSounds_.jumps.start + index]};

    effect.varyVolume();
    effect.varyPitch();

    playFootstepEffect();
    effect.play(true);
}

std::vector<SoundEffectEntry> AudioPlayer::createLandings(const std::vector<SoundEffectEntry> &footsteps,
                                                          float volumeMultiplier, float pitchMultiplier)
{
    std::vector<SoundEffectEntry> landings{footsteps};

    // Reserve one extra landing sound if footsteps size is one, since landings require two footstep sounds
    if (footsteps.size() == 1)
    {
        landings.reserve(2);
        landings.emplace_back(footsteps.front());
    }

    for (SoundEffectEntry &landing : landings)
    {
        landing.volumeLow *= volumeMultiplier;
        landing.volumeHigh *= volumeMultiplier;
        landing.pitchLow *= pitchMultiplier;
        landing.pitchHigh *= pitchMultiplier;
    }

    return landings;
}