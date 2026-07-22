#include "scene/island_scene/island_scene.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "application/graphics_settings.h"
#include "models/animation/animation.h"
#include "scene/island_scene/seagull.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

namespace
{
namespace Butterfly
{
// Update butterfly positions and animations
constexpr float speed{5.0f};
constexpr float heightMultiplier{0.04f};
constexpr float bobSpeed{79.6227f};
constexpr float radius{8.0f};
constexpr float largeHeightWave{0.65f};
} // namespace Butterfly

namespace SeagullConstants
{
// How close the player can get before they fly away
constexpr float panicDist{12.0f};
constexpr float blendTime{0.3f};
constexpr float restTime{blendTime * 2.0f};
constexpr float panicTime{7.0f};
} // namespace SeagullConstants

namespace SpaceModel
{
constexpr float degreesPerSecond{2.0f};
constexpr glm::vec3 oddAxisAngle{0.2926f, 0.8048f, 0.5164f};
constexpr glm::vec3 oddAxisAngle2{-0.3197f, 0.7975f, 0.5117f};
} // namespace SpaceModel
} // namespace

IslandScene::IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                         const std::filesystem::path &jsonFile)
    : Scene(textureManager, graphicsSettings, jsonFile), seagull_{getEntry("seagull").worldTransforms.front(),
                                                                  Seagull::State::resting,
                                                                  SeagullConstants::panicDist,
                                                                  5.0f, // height
                                                                  15.0f,
                                                                  2.0f, // radius
                                                                  SeagullConstants::restTime}
{
    // Cache original positions so that animated models can return or calculate updated positions
    Entry &butterfly{editEntry("butterfly")};
    butterflyOriginalPos_.reserve(butterfly.worldTransforms.size());

    for (const Transform &transform : butterfly.worldTransforms)
        butterflyOriginalPos_.emplace_back(transform.getPosition());

    // Set starting animations for first frame
    Animation &fly{getAnimation(butterfly, "fly")};
    butterfly.entryAnimation->animator.setAnimation(fly);

    Entry &seagull{editEntry("seagull")};
    Animation &eat{getAnimation(seagull, "eat")};
    seagull.entryAnimation->animator.setAnimation(eat);
}

void IslandScene::onUpdate(float time, float deltaTime)
{
    Entry &spaceModel{editEntry("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.worldTransforms.size(); ++i)
    {
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(SpaceModel::degreesPerSecond * deltaTime,
                                                            SpaceModel::oddAxisAngle);
        spaceModel.worldTransforms[i].rotateAroundAxisLocal(SpaceModel::degreesPerSecond * deltaTime,
                                                            SpaceModel::oddAxisAngle2);
    }

    Entry &butterfly{editEntry("butterfly")};
    float baseAngle{glm::radians(time * Butterfly::speed)};

    for (std::size_t i{0}; i < butterfly.worldTransforms.size(); ++i)
    {
        const glm::vec3 originalPos{butterflyOriginalPos_[i]};
        const bool reversePattern{static_cast<int>(originalPos.x) % 2 == 1};
        const float angle{reversePattern ? -baseAngle : baseAngle};

        const float sinTheta{glm::sin(angle)};
        const float cosTheta{glm::cos(angle)};

        // Figure 8 (Lemniscate of Gerono)
        const glm::vec3 newPos{Butterfly::radius * sinTheta,
                               glm::sin(angle * Butterfly::bobSpeed) * Butterfly::heightMultiplier,
                               Butterfly::radius * (sinTheta * cosTheta)};
        const glm::vec3 heightModifier{0.0f, cosTheta * Butterfly::largeHeightWave, 0.0f};

        Transform &transform{butterfly.worldTransforms[i]};
        transform.setAbsoluteTranslation(originalPos + newPos + heightModifier);

        // Rotate to match new position
        const float modelFlip{glm::radians(reversePattern ? 0.0f : 180.0f)};
        float yaw{glm::atan(cosTheta, glm::cos(2.0f * angle))};
        transform.setAbsoluteRotation(glm::angleAxis(yaw + modelFlip, WorldVectors::worldUp));
    }

    butterfly.entryAnimation->animator.update(getSkeleton(butterfly), butterfly.primitives, deltaTime);

    seagull_.update(camera_.getPosition(), deltaTime);
    Entry &seagullEntry{editEntry("seagull")};

    if (seagull_.stateChanged())
    {
        switch (seagull_.getState())
        {
            using enum Seagull::State;

        case resting:
            seagullEntry.entryAnimation->animator.setTransition(getAnimation(seagullEntry, "eat"),
                                                                SeagullConstants::blendTime);
            break;

        case panicking:
            seagullEntry.entryAnimation->animator.setTransition(getAnimation(seagullEntry, "fly"),
                                                                SeagullConstants::blendTime);
            break;
        }
    }

    Transform &seagullTransform{seagullEntry.worldTransforms.front()};
    seagullTransform.setAbsoluteRotation(seagull_.getRotation());
    seagullTransform.setAbsoluteTranslation(seagull_.getPosition());

    seagullEntry.entryAnimation->animator.update(getSkeleton(seagullEntry), seagullEntry.primitives, deltaTime);
}