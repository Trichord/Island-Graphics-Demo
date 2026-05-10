#include "texture_loader.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "thread_pool.h"

TextureLoader::TextureLoader(ThreadPool &threadPool, const std::vector<std::filesystem::path> &filepaths)
    : numImages_{filepaths.size()}, numImagesLoaded_{0}
{
    loadedTextures_.resize(numImages_);

    // Queue up all filepaths in the thread pool
    for (unsigned int i = 0; i < filepaths.size(); ++i)
    {
        std::filesystem::path path = filepaths[i];
        threadPool.enqueue([this, path, i]() { loadFromFile(path, i); });
    }

    // Wait for all images to be loaded before leaving constructor
    {
        std::unique_lock<std::mutex> lock(imagesMutex_);
        condition_.wait(lock, [this] { return numImagesLoaded_ == numImages_; });
    }
}

std::vector<gli::texture> TextureLoader::getTextures()
{
    if (texturesReleased_)
    {
        std::cerr << "ERROR: Multiple texture fetches from a TextureLoader" << std::endl;
        throw std::logic_error("TextureLoader::getTextures() already called");
    }

    texturesReleased_ = true;
    return std::move(loadedTextures_);
}

void TextureLoader::loadFromFile(const std::filesystem::path &path, std::size_t index)
{
    gli::texture texture{gli::load(path.string().c_str())};

    if (texture.empty())
        std::cerr << "ERROR: Texture failed to load from " << path.string() << std::endl;

    // Lock and insert. Empty texture will be handled by the texture manager
    {
        std::unique_lock<std::mutex> lock{imagesMutex_};
        loadedTextures_[index] = texture;

        ++numImagesLoaded_;

        if (numImagesLoaded_ == numImages_)
            condition_.notify_one();
    }
}