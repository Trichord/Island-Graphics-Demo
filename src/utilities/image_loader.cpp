#define STB_IMAGE_IMPLEMENTATION

#include "image_loader.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "stb_image.h"
#include "thread_pool.h"

ImageLoader::ImageLoader(const std::filesystem::path& filepath)
    : numImages{1}, numImagesLoaded{0} {
  loadedImages.resize(numImages);
  loadFromFile(filepath, 0);
}

ImageLoader::ImageLoader(ThreadPool& threadPool,
                         const std::filesystem::path& filepath)
    : numImages{1}, numImagesLoaded{0} {
  loadedImages.resize(numImages);

  threadPool.enqueue([this, filepath]() { loadFromFile(filepath, 0); });

  // Wait for all images to be loaded before leaving constructor
  {
    std::unique_lock<std::mutex> lock(imagesMutex);
    condition.wait(lock, [this] { return numImagesLoaded == numImages; });
  }
}

ImageLoader::ImageLoader(ThreadPool& threadPool,
                         const std::vector<std::filesystem::path>& filepaths)
    : numImages{filepaths.size()}, numImagesLoaded{0} {
  loadedImages.resize(numImages);

  // Queue up all filepaths in the thread pool
  for (unsigned int i = 0; i < filepaths.size(); ++i) {
    std::filesystem::path path = filepaths[i];
    threadPool.enqueue([this, path, i]() { loadFromFile(path, i); });
  }

  // Wait for all images to be loaded before leaving constructor
  {
    std::unique_lock<std::mutex> lock(imagesMutex);
    condition.wait(lock, [this] { return numImagesLoaded == numImages; });
  }
}

ImageLoader::~ImageLoader() { freeImages(); }

const std::vector<LoadedImage>& ImageLoader::getImageData() const {
  return loadedImages;
}

void ImageLoader::freeImages() {
  for (auto& image : loadedImages)
    if (image.data) {
      stbi_image_free(image.data);
      image.data = nullptr;
    }
}

void ImageLoader::loadFromFile(const std::filesystem::path& path,
                               unsigned int index) {
  int width{0};
  int height{0};
  int nrChannels{0};
  unsigned char* data{
      stbi_load(path.string().c_str(), &width, &height, &nrChannels, 0)};

  if (!data)
    std::cerr << "STBI Failed to load image at: " << path.string() << std::endl;

  // Lock and insert.  If stbi_load fails, we get a nullptr which is handled by
  // model.h
  {
    std::unique_lock<std::mutex> lock{imagesMutex};
    loadedImages[index] = LoadedImage{data, width, height, nrChannels};

    ++numImagesLoaded;

    if (numImagesLoaded == numImages) condition.notify_one();
  }
}