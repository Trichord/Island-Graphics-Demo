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
    // Cache original positions so that animated models can return or calculate updated positions
    Entry &butterfly{editEntry("butterfly")};
    butterflyOriginalPos_.reserve(butterfly.worldTransforms.size());

    for (const Transform &transform : butterfly.worldTransforms)
        butterflyOriginalPos_.emplace_back(transform.getPosition());

    // Set starting animations for first frame
    Animation &fly{getAnimation(butterfly, "fly")};
    butterfly.entryAnimation->animator.setAnimation(fly);
}

void IslandScene::onUpdate(float time, float deltaTime)
{
    Entry &spaceModel{editEntry("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.worldTransforms.size(); ++i)
    {
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(degreesPerSecond * deltaTime, oddAxisAngle);
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(degreesPerSecond * deltaTime, oddAxisAngle2);
    }

    Entry &butterfly{editEntry("butterfly")};

    // Negative to orient the model properly
    float baseAngle{-glm::radians(time * butterflySpeed)};

    for (std::size_t i{0}; i < butterfly.worldTransforms.size(); ++i)
    {
        const glm::vec3 originalPos{butterflyOriginalPos_[i]};
        const bool reversePattern{static_cast<int>(originalPos.x) % 2 == 0};
        const float angle{reversePattern ? -baseAngle : baseAngle};

        const float sinTheta{glm::sin(angle)};
        const float cosTheta{glm::cos(angle)};

        // Figure 8 (Lemniscate of Gerono)
        const glm::vec3 newPos{radius * sinTheta, glm::sin(angle * bobSpeed) * heightMultiplier,
                               radius * (sinTheta * cosTheta)};
        const glm::vec3 heightModifier{0.0f, cosTheta * largeHeightWave, 0.0f};

        Transform &transform{butterfly.worldTransforms[i]};
        transform.setAbsoluteTranslation(originalPos + newPos + heightModifier);

        // Rotate to match new position
        const float modelFlip{reversePattern ? 0.0f : 180.0f};

        float yaw{glm::degrees(glm::atan(cosTheta, glm::cos(2.0f * angle)))};
        transform.setAbsoluteRotation(glm::vec3{0.0f, yaw + modelFlip, 0.0f});
    }

    butterfly.entryAnimation->animator.update(getSkeleton(butterfly), butterfly.primitives, deltaTime);
}
