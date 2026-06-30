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
    Entry &butterfly{getEntry("butterfly")};
    butterflyOriginalPos_.reserve(butterfly.worldTransforms.size());

    for (Transform &transform : butterfly.worldTransforms)
        butterflyOriginalPos_.emplace_back(transform.getPosition());
}

void IslandScene::onUpdate(float time, float deltaTime)
{
    constexpr float degreesPerSecond{2.0f};
    constexpr glm::vec3 oddAxisAngle{0.2926f, 0.8048f, 0.5164f};
    constexpr glm::vec3 oddAxisAngle2{-0.3197f, 0.7975f, 0.5117f};

    // Rotate the fancy space model
    Entry &spaceModel{getEntry("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.worldTransforms.size(); ++i)
    {
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(degreesPerSecond * deltaTime, oddAxisAngle);
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(degreesPerSecond * deltaTime, oddAxisAngle2);
    }

    // Update butterfly positions and animations
    constexpr float butterflySpeed{4.0f};
    constexpr float heightMultiplier{0.075f};
    constexpr float bobSpeed{79.6227f};
    constexpr float radius{8.0f};
    constexpr float largeHeightWave{0.65f};

    Entry &butterfly{getEntry("butterfly")};

    // Negative to move the butterfly "forward"
    const float angle{-glm::radians(time * butterflySpeed)};

    for (std::size_t i{0}; i < butterfly.worldTransforms.size(); ++i)
    {
        Transform &transform{butterfly.worldTransforms[i]};

        const glm::vec3 originalPos{butterflyOriginalPos_[i]};
        const glm::vec3 newPos{glm::sin(angle) * radius, glm::sin(angle * bobSpeed) * heightMultiplier,
                               glm::cos(angle) * radius};

        const glm::vec3 heightModifier{0.0f, glm::cos(angle) * largeHeightWave, 0.0f};

        transform.setAbsoluteTranslation(originalPos + newPos + heightModifier);

        float yaw{glm::degrees(glm::atan(glm::cos(angle), -glm::sin(angle)))};
        transform.setAbsoluteRotation(glm::vec3{0.0f, yaw + 180.0f, 0.0f});
    }

    playAnimation(butterfly, "fly", deltaTime * 0.25f);

    // Flag updated models
    updateHandle(spaceModel.handle);
    updateHandle(butterfly.handle);
}
