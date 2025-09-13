#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include "stb_image.h"
#include "thread_pool.h"
#include "texture.h"

extern ThreadPool threadPool;

struct ImageData {
	unsigned char *data;
	int width;
	int height;
	int nrChannels;
	TextureType textureType;

	ImageData() = default;
	ImageData(unsigned char *d, int w, int h, int nr, TextureType tex) : data(d), width(w), height(h), nrChannels(nr), textureType(tex) {};
};

class ImageLoader {
public:
	//Queue up all filepaths in the thread pool
	ImageLoader(const std::unordered_map<std::filesystem::path, TextureType> &inputMap) : numImages(inputMap.size()), numImagesLoaded(0)  {
		
		for (auto const& [path, type] : inputMap) {
			threadPool.enqueue([this, path, type]() {
				loadFromFile(path, type);
				});
		}

		//Wait for all images to be loaded before leaving constructor
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			condition.wait(lock, [this] {
				return numImagesLoaded == numImages; });
		}
	}
	
	ImageLoader(const std::vector<std::filesystem::path>& filepaths) : numImages(filepaths.size()), numImagesLoaded(0) {
		
		for (auto const& path : filepaths)
			threadPool.enqueue([this, path]() {
			loadFromFile(path, TEXTURE_NONE);
				});
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			condition.wait(lock, [this] {
				return numImagesLoaded == numImages; });
		}
	}
	
	~ImageLoader() {
		freeImages();
	}

	const std::unordered_map<std::filesystem::path, ImageData>&getImageData() const {
		return loadedImages;
	}

	void freeImages() {
		for (auto& [path, image] : loadedImages)
			if (image.data != nullptr) {
				stbi_image_free(image.data);
				image.data = nullptr;
			}
	}

private:
	size_t numImages;
	std::atomic<unsigned int> numImagesLoaded;
	std::unordered_map<std::filesystem::path, ImageData> loadedImages;
	std::mutex imagesMutex;
	std::condition_variable condition;

	void loadFromFile(const std::filesystem::path &path, TextureType texType) {
		int width, height, nrChannels;
		unsigned char *image = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 0);
		
		//Lock and insert.  If stbi_load fails, we get a nullptr which is handled by model.h
		{
			std::unique_lock<std::mutex> lock(imagesMutex);
			loadedImages.emplace(path, ImageData{ image, width, height, nrChannels, texType});
		}
		numImagesLoaded++;
			if (numImagesLoaded == numImages)
				condition.notify_one();
	}
};