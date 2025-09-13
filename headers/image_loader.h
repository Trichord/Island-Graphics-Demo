#pragma once

#include "thread_pool.h"
#include "texture.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>

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
	ImageLoader(const std::unordered_map<std::filesystem::path, TextureType>& inputMap);
	ImageLoader(const std::vector<std::filesystem::path>& filepaths);	
	~ImageLoader();

	const std::unordered_map<std::filesystem::path, ImageData>& getImageData() const;
	void freeImages();

private:
	size_t numImages;
	std::atomic<unsigned int> numImagesLoaded;
	std::unordered_map<std::filesystem::path, ImageData> loadedImages;
	std::mutex imagesMutex;
	std::condition_variable condition;

	void loadFromFile(const std::filesystem::path& path, TextureType texType);
};