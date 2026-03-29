#define STB_IMAGE_IMPLEMENTATION

#include "image_loader.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "stb_image/stb_image.h"
#include "thread_pool.h"

ImageLoader::ImageLoader(const std::filesystem::path &filepath) : numImages_{1}, numImagesLoaded_{0}
{
    loadedImages_.resize(numImages_);
    loadFromFile(filepath, 0);
}

ImageLoader::ImageLoader(ThreadPool &threadPool, const std::filesystem::path &filepath)
    : numImages_{1}, numImagesLoaded_{0}
{
    loadedImages_.resize(numImages_);

    threadPool.enqueue([this, filepath]() { loadFromFile(filepath, 0); });

    // Wait for all images to be loaded before leaving constructor
    {
        std::unique_lock<std::mutex> lock(imagesMutex_);
        condition_.wait(lock, [this] { return numImagesLoaded_ == numImages_; });
    }
}

ImageLoader::ImageLoader(ThreadPool &threadPool, const std::vector<std::filesystem::path> &filepaths)
    : numImages_{filepaths.size()}, numImagesLoaded_{0}
{
    loadedImages_.resize(numImages_);

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

ImageLoader::~ImageLoader()
{
    freeImages();
}

const std::vector<LoadedImage> &ImageLoader::getImageData() const
{
    return loadedImages_;
}

void ImageLoader::freeImages()
{
    for (auto &image : loadedImages_)
        if (image.data)
        {
            stbi_image_free(image.data);
            image.data = nullptr;
        }
}

void ImageLoader::loadFromFile(const std::filesystem::path &path, unsigned int index)
{
    int width{0};
    int height{0};
    int nrChannels{0};
    unsigned char *data{stbi_load(path.string().c_str(), &width, &height, &nrChannels, 0)};

    if (!data)
        std::cerr << "STBI Failed to load image at: " << path.string() << std::endl;

    // Lock and insert.  If stbi_load fails, we get a nullptr which is handled by
    // model.h
    {
        std::unique_lock<std::mutex> lock{imagesMutex_};
        loadedImages_[index] = LoadedImage{data, width, height, nrChannels};

        ++numImagesLoaded_;

        if (numImagesLoaded_ == numImages_)
            condition_.notify_one();
    }
}