#include "utilities/texture_loader.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utilities/ktx_texture.h"
#include "utilities/thread_pool.h"

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

std::vector<KTXTexture> TextureLoader::getTextures()
{
    if (texturesReleased_)
    {
        std::cerr << "ERROR: Multiple texture fetches from a TextureLoader\n";
        throw std::logic_error("TextureLoader::getTextures() already called");
    }

    texturesReleased_ = true;
    return std::move(loadedTextures_);
}

void TextureLoader::loadFromFile(const std::filesystem::path &path, std::size_t index)
{
    // Default construct an empty texture to ensure the 1:1 mapping of filepaths coming in and textures going out
    KTXTexture texture;

    if (!path.empty() && std::filesystem::exists(path))
        texture = KTXTexture(path.string().c_str());

    // Lock and insert. Empty texture will be handled by the texture manager
    {
        std::unique_lock<std::mutex> lock{imagesMutex_};
        loadedTextures_[index] = std::move(texture);

        ++numImagesLoaded_;

        if (numImagesLoaded_ == numImages_)
            condition_.notify_one();
    }
}