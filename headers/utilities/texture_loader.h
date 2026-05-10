#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <vector>

#include "gli/gli.hpp"
#include "thread_pool.h"

/*
ImageLoader takes in a vector of image filepaths, and returns a vector of
textures loaded from disk in the same order. Not intended for loading single textures.
*/

class TextureLoader
{
  public:
    TextureLoader(ThreadPool &threadPool, const std::vector<std::filesystem::path> &filepaths);

    [[nodiscard]] std::vector<gli::texture> getTextures();

  private:
    bool texturesReleased_{false};

    std::mutex imagesMutex_;
    std::condition_variable condition_;

    std::size_t numImages_{0};
    std::atomic<std::size_t> numImagesLoaded_{0};
    std::vector<gli::texture> loadedTextures_;

    void loadFromFile(const std::filesystem::path &path, std::size_t index);
};