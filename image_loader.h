#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include "stb_image.h"
#include "thread_pool.h"

extern ThreadPool threadPool;

class ImageLoader {
public:
	//Queue up all filepaths in the thread pool
	ImageLoader(const std::unordered_map<std::string, unsigned char *> &inputMap) : numImages(inputMap.size()), imagesLoaded(0)  {
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

	const std::unordered_map<std::string, unsigned char *> &getImageMap() const {
		return loadedImages;
	}

private:
	unsigned int numImages;
	unsigned int imagesLoaded;
	std::unordered_map<std::string, unsigned char *> loadedImages;
	std::mutex imagesMutex;
	std::condition_variable condition;

	void loadFromFile(const std::string &path) {
		int width, height, nrChannels;
		unsigned char *image = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
		
		//Lock and insert.  If stbi_load fails, we get a nullptr which is treated as a failed texture load by model.h
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			loadedImages.insert({ path, image });
			imagesLoaded++;
			if (imagesLoaded == numImages)
				condition.notify_one();
		}
	}

	void freeImages() {
		for (auto &pair : loadedImages)
			if (pair.second != nullptr)
				stbi_image_free(pair.second);
	}
};