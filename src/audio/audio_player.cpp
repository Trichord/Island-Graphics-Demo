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

namespace
{
std::minstd_rand &rnd()
{
    static std::minstd_rand gen{std::random_device{}()};
    return gen;
}

int randomIndex(std::size_t size)
{
    return std::uniform_int_distribution<int>(0, static_cast<int>(size) - 1)(rnd());
}

float randomFloat(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(rnd());
}
} // namespace

AudioPlayer::AudioPlayer(const std::filesystem::path &jsonFile)
{
    using namespace nlohmann;

    constexpr ma_uint32 footstepFlags{MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION};

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

    triggers_.reserve(audio.audioEntries.size());
    hasPlayed_.resize(audio.audioEntries.size());
    footsteps_.reserve(audio.footsteps.soundPaths.size());

    // Reserve enough space that so that a single footstep can be duplicated without invalidating internal pointers
    landings_.reserve(std::max(audio.footsteps.soundPaths.size(), std::size_t{2}));

    for (const AudioEntry &entry : audio.audioEntries)
    {
        triggers_.emplace_back(Trigger{
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

        ma_sound &sound{triggers_.back().sound};
        ma_result result;
        result =
            ma_sound_init_from_file(&audioEngine_, entry.soundPath.string().c_str(), flags, nullptr, nullptr, &sound);

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load sound from " + entry.soundPath.string() + " : " +
                                     ma_result_description(result));
        }

        glm::vec3 pos{entry.zone.getCenter()};
        ma_sound_set_position(&sound, pos.x, pos.y, pos.z);

        if (entry.loop)
            ma_sound_set_looping(&sound, MA_TRUE);

        if (entry.playImmediate)
            ma_sound_start(&sound);

        ma_sound_set_volume(&sound, entry.volume);
    }

    auto addSoundEffect = [](ma_engine &engine, std::vector<SoundEffect> &sounds, const std::filesystem::path &path,
                             ma_uint32 flags, float volume) {
        SoundEffect &soundEffect{sounds.emplace_back(SoundEffect{.volume = volume, .sound = {}})};

        ma_result result;
        result = ma_sound_init_from_file(&engine, path.string().c_str(), flags, nullptr, nullptr, &soundEffect.sound);

        if (result != MA_SUCCESS)
        {
            std::cerr << ma_result_description(result) << std::endl;
            throw std::runtime_error("Failed to load soundEffect from " + path.string() + ": " +
                                     ma_result_description(result));
        }
    };

    for (const std::filesystem::path &path : audio.footsteps.soundPaths)
    {
        addSoundEffect(audioEngine_, footsteps_, path, footstepFlags, audio.footsteps.volume);
        addSoundEffect(audioEngine_, landings_, path, footstepFlags, audio.footsteps.volume);
    }

    // Landings need two footsteps to play simultaneously
    if (footsteps_.size() == 1)
        addSoundEffect(audioEngine_, landings_, audio.footsteps.soundPaths.front(), footstepFlags,
                       audio.footsteps.volume);
}

AudioPlayer::~AudioPlayer()
{
    for (Trigger &zone : triggers_)
        ma_sound_uninit(&zone.sound);

    for (SoundEffect &footstep : footsteps_)
        ma_sound_uninit(&footstep.sound);

    for (SoundEffect &landing : landings_)
        ma_sound_uninit(&landing.sound);

    ma_engine_uninit(&audioEngine_);
}

void AudioPlayer::update(const glm::vec3 &listenerPos, const glm::vec3 &listenerFront)
{
    if (triggers_.empty())
        return;

    for (std::size_t i{0}; i < triggers_.size(); ++i)
    {
        Trigger &trigger{triggers_[i]};

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

    if (footsteps_.empty())
        return;

    const int index{randomIndex(footsteps_.size())};
    SoundEffect &soundEffect{footsteps_[index]};

    // Only make footsteps randomly softer. Normal footstep volume is already plenty.
    const float volume{randomFloat(soundEffect.volume - volumeVariation, soundEffect.volume)};
    const float pitch{randomFloat(1.0f - pitchVariation, 1.0f + pitchVariation)};

    playSound(soundEffect.sound, volume, pitch);
}

void AudioPlayer::playLanding()
{
    constexpr float landingPitch{0.7f};
    constexpr float pitchVariation{0.15f};
    constexpr float volumeLow{0.2f};
    constexpr float volumeHigh{0.4f};

    if (landings_.empty())
        return;

    const int index{randomIndex(landings_.size())};
    int otherIndex{randomIndex(landings_.size() - 1)};

    if (otherIndex >= index)
        ++otherIndex;

    SoundEffect &first{landings_[index]};
    SoundEffect &second{landings_[otherIndex]};

    // Landing sounds should be lower pitched and louder
    const float firstVolume{randomFloat(first.volume + volumeLow, first.volume + volumeHigh)};
    const float secondVolume{randomFloat(second.volume + volumeLow, second.volume + volumeHigh)};
    const float firstPitch{randomFloat(landingPitch - pitchVariation, landingPitch + pitchVariation)};
    const float secondPitch{randomFloat(landingPitch - pitchVariation, landingPitch + pitchVariation)};

    playSound(first.sound, firstVolume, firstPitch);
    playSound(second.sound, secondVolume, secondPitch);
}

void AudioPlayer::playSound(ma_sound &sound, float volume, float pitch)
{
    ma_sound_set_volume(&sound, volume);
    ma_sound_set_pitch(&sound, pitch);
    ma_sound_seek_to_pcm_frame(&sound, 0);
    ma_sound_start(&sound);
}