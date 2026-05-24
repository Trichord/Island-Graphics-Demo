#include "island_scene.h"

#include <filesystem>
#include <vector>

#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "scene.h"
#include "texture_manager.h"
#include "transform.h"

IslandScene::IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                         const std::filesystem::path &jsonFile)
    : Scene(textureManager, graphicsSettings, jsonFile)
{
}

void IslandScene::onUpdate(float deltaTime)
{
    // Rotate the fancy space model
    Entry &spaceModel{getEntryByName("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.transforms.size(); ++i)
        spaceModel.transforms[i].rotateAroundAxis(15.0f * deltaTime, glm::vec3{0.34f, 0.935f, 0.6f});

    updatedModels_.insert(spaceModel.handle);
}