#define STB_IMAGE_IMPLEMENTATION

#include "../headers/image_loader.h"
#include "../headers/thread_pool.h"
#include "stb_image.h"
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include <iostream>

ImageLoader::ImageLoader(const std::vector<std::filesystem::path>& filepaths) : numImages(filepaths.size()), numImagesLoaded(0) {

	loadedImages.resize(numImages);

	//Queue up all filepaths in the thread pool
	for (unsigned int i = 0; i < filepaths.size(); ++i) {
		std::filesystem::path path = filepaths[i];
		threadPool.enqueue([this, path, i]() {
			loadFromFile(path, i);
			});
	}
	
	//Wait for all images to be loaded before leaving constructor
	{
		std::unique_lock<std::mutex> lock(imagesMutex);
		condition.wait(lock, [this] {
			return numImagesLoaded == numImages; });
	}
}

ImageLoader::~ImageLoader() {
	freeImages();
}

const std::vector<ImageData>& ImageLoader::getImageData() const {
	return loadedImages;
}

void ImageLoader::freeImages() {
	for (auto& image : loadedImages)
		if (image.data != nullptr) {
			stbi_image_free(image.data);
			image.data = nullptr;
		}
}

void ImageLoader::loadFromFile(const std::filesystem::path& path, unsigned int index) {
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 0);

	if(!data)
		std::cerr << "STBI Failed to load image at: " << path.string() << std::endl;

	//Lock and insert.  If stbi_load fails, we get a nullptr which is handled by model.h
	{
		std::unique_lock<std::mutex> lock(imagesMutex);
		loadedImages[index] = ImageData(data, width, height, nrChannels);
	}

	numImagesLoaded++;

	if (numImagesLoaded == numImages)
		condition.notify_one();
}