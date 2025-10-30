#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <vector>

#include "thread_pool.h"

extern ThreadPool threadPool;

struct ImageData {
  unsigned char* data{nullptr};
  int width{0};
  int height{0};
  int nrChannels{1};

  ImageData() = default;
  ImageData(unsigned char* d, int w, int h, int nr)
      : data(d), width(w), height(h), nrChannels(nr) {};
};

/*
ImageLoader takes in a vector of image filepaths, and returns a vector of
ImageData from stb_image in the exact same order.  It's multi-threaded and
always returns a vector as it was never intended for single-image loading
*/

class ImageLoader {
 public:
  explicit ImageLoader(const std::filesystem::path& filepath);
  explicit ImageLoader(const std::vector<std::filesystem::path>& filepaths);
  ~ImageLoader();

  const std::vector<ImageData>& getImageData() const;
  void freeImages();

 private:
  size_t numImages{0};
  std::atomic<unsigned int> numImagesLoaded{0};
  std::vector<ImageData> loadedImages;
  std::mutex imagesMutex;
  std::condition_variable condition;

  void loadFromFile(const std::filesystem::path& path,
                    unsigned int assetImageIndex);
};