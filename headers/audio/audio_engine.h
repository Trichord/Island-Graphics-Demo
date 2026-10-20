#pragma once

#include <miniaudio.h>
#include <stdexcept>
#include <iostream>

/**
 * @brief Simple RAII wrapper around MiniAudio's audio engine.
 */
struct AudioEngine
{
    ma_engine engine;

    AudioEngine()
    {
        ma_result engineInit{ma_engine_init(nullptr, &engine)};

        if (engineInit != MA_SUCCESS)
        {
            std::cerr << "Error: Failed to initialize audio engine\n";
            throw std::runtime_error("ma_engine init failure");
        }
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