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

    zones_.reserve(audio.audioEntries.size());
    hasPlayed_.resize(audio.audioEntries.size());
    footsteps_.reserve(audio.footsteps.soundPaths.size());

    for (const AudioEntry &entry : audio.audioEntries)
    {
        zones_.emplace_back(Zone{
            .zone = entry.zone,
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

        ma_sound &sound{zones_.back().sound};
        ma_result result;
        result =
            ma_sound_init_from_file(&audioEngine_, entry.soundPath.string().c_str(), flags, nullptr, nullptr, &sound);

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load sound");
        }

        if (entry.loop)
            ma_sound_set_looping(&sound, MA_TRUE);

        if (entry.playImmediate)
            ma_sound_start(&sound);
    }

    for (const std::filesystem::path &path : audio.footsteps.soundPaths)
    {
        footsteps_.emplace_back(ma_sound{});
        ma_result result;
        result = ma_sound_init_from_file(&audioEngine_, path.string().c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                         &footsteps_.back());

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load footstep sound");
        }
    }
}

AudioPlayer::~AudioPlayer()
{
    for (Zone &zone : zones_)
        ma_sound_uninit(&zone.sound);

    for (ma_sound &sound : footsteps_)
        ma_sound_uninit(&sound);

    ma_engine_uninit(&audioEngine_);
}

void AudioPlayer::update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront)
{
    if (zones_.empty())
        return;

    for (std::size_t i{0}; i < zones_.size(); ++i)
    {
        Zone &zone{zones_[i]};
        const bool wasEnabled{zone.enabled};

        zone.enabled = zone.zone.pointInside(listenerPos);

        if (wasEnabled != zone.enabled)
        {
            if (zone.enabled)
            {
                if (zone.oneShot && hasPlayed_[i])
                    continue;

                ma_sound_start(&zone.sound);

                if (ma_sound_at_end(&zone.sound))
                    hasPlayed_[i] = true;
            }
            else
            {
                ma_sound_stop(&zone.sound);

                if (!ma_sound_is_looping(&zone.sound))
                    ma_sound_seek_to_pcm_frame(&zone.sound, 0);
            }
        }
    }

    ma_engine_listener_set_position(&audioEngine_, 0, listenerPos.x, listenerPos.y, listenerPos.z);
    ma_engine_listener_set_direction(&audioEngine_, 0, listenerFront.x, listenerFront.y, listenerFront.z);
}

void AudioPlayer::playFootstep()
{
    constexpr float pitchVariation{0.1f};
    static std::minstd_rand gen{std::random_device{}()};

    if (footsteps_.empty())
        return;

    int soundIndex{std::uniform_int_distribution<int>(0, footsteps_.size())(gen)};
    float randomPitch{std::uniform_real_distribution<float>(-pitchVariation, pitchVariation)(gen)};

    ma_sound &sound{footsteps_[soundIndex]};

    if (ma_sound_at_end(&sound))
        ma_sound_seek_to_pcm_frame(&sound, 0);

    ma_sound_set_pitch(&sound, 1.0f + randomPitch);
    ma_sound_start(&sound);
}