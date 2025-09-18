#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../headers/image_loader.h"
#include "../headers/thread_pool.h"
#include "../headers/texture.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>

//Queue up all filepaths in the thread pool
ImageLoader::ImageLoader(const std::unordered_map<std::filesystem::path, unsigned int>& inputMap) : numImages(inputMap.size()), numImagesLoaded(0) {

	loadedImages.resize(numImages);

	for (auto const& [path, assetImageIndex] : inputMap) {
		threadPool.enqueue([this, path, assetImageIndex]() {
			loadFromFile(path, assetImageIndex);
			});
	}

	//Wait for all images to be loaded before leaving constructor
	{
		std::unique_lock<std::mutex> lock(imagesMutex);
		condition.wait(lock, [this] {
			return numImagesLoaded == numImages; });
	}
}

ImageLoader::ImageLoader(const std::vector<std::filesystem::path>& filepaths) : numImages(filepaths.size()), numImagesLoaded(0) {

	loadedImages.resize(numImages);
	unsigned int index = 0;

	for (auto const& path : filepaths) {
		threadPool.enqueue([this, path, index]() {
			loadFromFile(path, index);
			});
		index++;
	}
	
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

void ImageLoader::loadFromFile(const std::filesystem::path& path, unsigned int assetImageIndex) {
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 0);

	//Lock and insert.  If stbi_load fails, we get a nullptr which is handled by model.h
	{
		std::unique_lock<std::mutex> lock(imagesMutex);
		loadedImages[assetImageIndex] = ImageData(data, width, height, nrChannels);
	}

	numImagesLoaded++;

	if (numImagesLoaded == numImages)
		condition.notify_one();
}