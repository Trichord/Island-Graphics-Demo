#include "scene/island_scene/island_scene.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "application/graphics_settings.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

IslandScene::IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                         const std::filesystem::path &jsonFile)
    : Scene(textureManager, graphicsSettings, jsonFile)
{
}

void IslandScene::onUpdate(float deltaTime)
{
    constexpr float degrees{2.0f};
    constexpr glm::vec3 oddAxisAngle{0.2926f, 0.8048f, 0.5164f};
    constexpr glm::vec3 oddAxisAngle2{-0.3197f, 0.7975f, 0.5117f};

    // Rotate the fancy space model
    Entry &spaceModel{getEntryByName("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.transforms.size(); ++i)
    { 
        spaceModel.transforms[i].rotateAroundAxisLocal(degrees * deltaTime, oddAxisAngle);
        spaceModel.transforms[i].rotateAroundAxisLocal(degrees * deltaTime, oddAxisAngle2);
    }

    updatedModels_.insert(spaceModel.handle);
}
