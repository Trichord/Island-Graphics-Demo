#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include "stb_image.h"
#include "thread_pool.h"
#include "mesh.h"

extern ThreadPool threadPool;

struct ImageData {
	unsigned char *data;
	int width;
	int height;
	int nrChannels;

	ImageData() = default;
	ImageData(unsigned char *d, int w, int h, int nr) : data(d), width(w), height(h), nrChannels(nr) {};
};

class ImageLoader {
public:
	//Queue up all filepaths in the thread pool
	ImageLoader(const std::unordered_map<std::string, Texture> &inputMap) : numImages(inputMap.size()), imagesLoaded(0)  {
		for (auto const & [path, _] : inputMap)
			threadPool.enqueue([this, path]() {
				loadFromFile(path);
			});

		//Wait for all images to be loaded before leaving constructor
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			condition.wait(lock, [this] {
				return imagesLoaded == numImages; 
				});
		}
	}

	~ImageLoader() {
		freeImages();
	}

	const std::unordered_map<std::string, ImageData> &getImageData() const {
		return loadedImages;
	}

	void freeImages() {
		for (auto &pair : loadedImages)
			if (pair.second.data != nullptr) {
				stbi_image_free(pair.second.data);
				pair.second.data = nullptr;
			}
	}

private:
	size_t numImages;
	unsigned int imagesLoaded;
	std::unordered_map<std::string, ImageData> loadedImages;
	std::mutex imagesMutex;
	std::condition_variable condition;

	void loadFromFile(const std::string &path) {
		int width, height, nrChannels;
		unsigned char *image = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
		
		//Lock and insert.  If stbi_load fails, we get a nullptr which is handled by model.h
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			loadedImages.emplace(path, ImageData{ image, width, height, nrChannels });
			imagesLoaded++;
			if (imagesLoaded == numImages)
				condition.notify_one();
		}
	}
};