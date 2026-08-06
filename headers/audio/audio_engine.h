#pragma once

#include <miniaudio.h>
#include <stdexcept>

struct AudioEngine
{
    ma_engine engine;

    AudioEngine()
    {
        ma_result engineInit{ma_engine_init(nullptr, &engine)};

        if (engineInit != MA_SUCCESS)
            throw std::runtime_error("Failed to initialize audio engine");
    }

    ~AudioEngine()
    {
        ma_engine_uninit(&engine);
    }

    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;
    AudioEngine(AudioEngine &&) = delete;
    AudioEngine &operator=(AudioEngine &&) = delete;
};