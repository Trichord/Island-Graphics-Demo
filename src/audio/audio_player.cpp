#include "audio/audio_player.h"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <miniaudio.h>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

#include "scene/audio_file.h"
#include "utilities/world_vectors.h"

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
    ma_result engineInit;
    engineInit = ma_engine_init(nullptr, &audioEngine_);

    if (engineInit != MA_SUCCESS)
    {
        std::cerr << ma_result_description(engineInit) << std::endl;
        throw std::runtime_error("Failed to initialize audio engine");
    }

    ma_engine_listener_set_world_up(&audioEngine_, 0, WorldVectors::worldUp.x, WorldVectors::worldUp.y,
                                    WorldVectors::worldUp.z);

    trigger_.reserve(audio.audioEntries.size());
    hasPlayed_.resize(audio.audioEntries.size());
    footsteps_.reserve(audio.footsteps.soundPaths.size());

    for (const AudioEntry &entry : audio.audioEntries)
    {
        trigger_.emplace_back(Trigger{
            .aabb = entry.zone,
            .enabled = entry.playImmediate,
            .oneShot = entry.oneShot,
        });

        ma_uint32 flags{0};
        if (entry.streaming)
            flags |= MA_SOUND_FLAG_STREAM;
        else
            flags |= MA_SOUND_FLAG_DECODE;

        if (!entry.spatialization)
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        ma_sound &sound{trigger_.back().sound};
        ma_result result;
        result =
            ma_sound_init_from_file(&audioEngine_, entry.soundPath.string().c_str(), flags, nullptr, nullptr, &sound);

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load sound");
        }

        glm::vec3 pos{entry.zone.getCenter()};
        ma_sound_set_position(&sound, pos.x, pos.y, pos.z);

        if (entry.loop)
            ma_sound_set_looping(&sound, MA_TRUE);

        if (entry.playImmediate)
            ma_sound_start(&sound);

        ma_sound_set_volume(&sound, entry.volume);
    }

    for (const std::filesystem::path &path : audio.footsteps.soundPaths)
    {
        footsteps_.emplace_back(Footstep{
            .volume = audio.footsteps.volume,
            .sound = ma_sound{},
        });

        ma_result result;
        result = ma_sound_init_from_file(&audioEngine_, path.string().c_str(),
                                         MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr,
                                         &footsteps_.back().sound);

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load footstep sound");
        }
    }
}

AudioPlayer::~AudioPlayer()
{
    for (Trigger &zone : trigger_)
        ma_sound_uninit(&zone.sound);

    for (Footstep &footstep : footsteps_)
        ma_sound_uninit(&footstep.sound);

    ma_engine_uninit(&audioEngine_);
}

void AudioPlayer::update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront)
{
    if (trigger_.empty())
        return;

    for (std::size_t i{0}; i < trigger_.size(); ++i)
    {
        Trigger &trigger{trigger_[i]};

        if (ma_sound_at_end(&trigger.sound))
            hasPlayed_[i] = true;

        if (trigger.oneShot && hasPlayed_[i] || trigger.oneShot && ma_sound_is_playing(&trigger.sound))
            continue;

        const bool wasEnabled{trigger.enabled};
        trigger.enabled = trigger.aabb.pointInside(listenerPos);

        if (wasEnabled != trigger.enabled)
        {
            if (trigger.enabled)
                ma_sound_start(&trigger.sound);
            else
            {
                ma_sound_stop(&trigger.sound);

                if (!ma_sound_is_looping(&trigger.sound))
                    ma_sound_seek_to_pcm_frame(&trigger.sound, 0);
            }
        }
    }

    ma_engine_listener_set_position(&audioEngine_, 0, listenerPos.x, listenerPos.y, listenerPos.z);
    ma_engine_listener_set_direction(&audioEngine_, 0, listenerFront.x, listenerFront.y, listenerFront.z);
}

void AudioPlayer::playFootstep()
{
    constexpr float pitchVariation{0.2f};
    constexpr float volumeVariation{0.4f};

    static std::minstd_rand gen{std::random_device{}()};

    if (footsteps_.empty())
        return;

    float randomPitch{std::uniform_real_distribution<float>(-pitchVariation, pitchVariation)(gen)};
    float randomVolume{std::uniform_real_distribution<float>(0.0f, volumeVariation)(gen)};
    int randomIndex{std::uniform_int_distribution<int>(0, footsteps_.size() - 1)(gen)};

    Footstep &footstep{footsteps_[randomIndex]};

    // Only make footsteps randomly softer. Normal footstep volume is already plenty.
    ma_sound_set_volume(&footstep.sound, footstep.volume - randomVolume);
    ma_sound_set_pitch(&footstep.sound, 1.0f + randomPitch);

    ma_sound_seek_to_pcm_frame(&footstep.sound, 0);
    ma_sound_start(&footstep.sound);
}