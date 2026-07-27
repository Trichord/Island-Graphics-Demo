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
constexpr float animationTimeScale{0.85f};
} // namespace Butterfly

namespace SeagullConstants
{
// How close the player can get before they fly away
constexpr float panicDist{20.0f};
constexpr float blendTime{0.3f};
constexpr float restTime{blendTime * 2.0f};
constexpr float orbitTime{20.0f};
constexpr float orbitHeight{15.0f};
constexpr float orbitRadius{10.0f};
} // namespace SeagullConstants

namespace SpaceModel
{
constexpr float degreesPerSecond{2.0f};
constexpr glm::vec3 oddAxisAngle{0.2926f, 0.8048f, 0.5164f};
constexpr glm::vec3 oddAxisAngle2{-0.3197f, 0.7975f, 0.5117f};
} // namespace SpaceModel

float randomFloat(float low, float high)
{
    static std::minstd_rand gen{std::random_device{}()};
    return std::uniform_real_distribution<float>(low, high)(gen);
}

} // namespace

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

    // Seed the gulls with some randomness for some variety in their interactions
    for (std::size_t i{0}; i < seagullNames_.size(); ++i)
    {
        Scene::Entry &seagull{editEntry(seagullNames_[i].data())};

        seagulls_[i] = Seagull{seagull.worldTransforms.front(),
                               Seagull::State::resting,
                               SeagullConstants::panicDist + randomFloat(-0.125f, 0.125f),
                               SeagullConstants::orbitHeight + randomFloat(-3.0f, 3.0f),
                               SeagullConstants::orbitTime + randomFloat(-2.0f, 8.0f),
                               SeagullConstants::orbitRadius + randomFloat(-2.0f, 2.0f),
                               SeagullConstants::restTime + randomFloat(0.0f, 0.2f)};

        seagull.entryAnimation->animator.setAnimation(randomSeagullIdle(seagull));

        // Simulate just a bit of a starting animation to desync
        seagull.entryAnimation->animator.update(getSkeleton(seagull), seagull.primitives, randomFloat(0.1f, 0.3f));
    }
}

void IslandScene::onUpdate(float time, float deltaTime)
{
    updateSpaceModel(editEntry("spaceModel"), deltaTime);
    updateButterfly(editEntry("butterfly"), time, deltaTime);

    for (std::size_t i{0}; i < seagullNames_.size(); ++i)
        updateSeagull(seagulls_[i], editEntry(seagullNames_[i]), deltaTime);
}

Animation &IslandScene::randomSeagullIdle(Scene::Entry &entry)
{
    float random{randomFloat(0.0f, 100.0f)};

    if (random <= 33.0f)
        return getAnimation(entry, "eat");
    else if (random <= 66.0f)
        return getAnimation(entry, "look");
    else
        return getAnimation(entry, "clearing");
}

void IslandScene::updateSpaceModel(Scene::Entry &entry, float deltaTime)
{
    for (std::size_t i{0}; i < entry.worldTransforms.size(); ++i)
    {
        entry.worldTransforms[i].rotateAroundAxisLocal(SpaceModel::degreesPerSecond * deltaTime,
                                                       SpaceModel::oddAxisAngle);
        entry.worldTransforms[i].rotateAroundAxisLocal(SpaceModel::degreesPerSecond * deltaTime,
                                                       SpaceModel::oddAxisAngle2);
    }
}

void IslandScene::updateButterfly(Scene::Entry &butterfly, float time, float deltaTime)
{
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

    butterfly.entryAnimation->animator.update(getSkeleton(butterfly), butterfly.primitives,
                                              deltaTime * Butterfly::animationTimeScale);
}

void IslandScene::updateSeagull(Seagull &seagull, Scene::Entry &entry, float deltaTime)
{
    seagull.update(camera_.getPosition(), deltaTime);

    if (seagull.stateChanged())
    {
        switch (seagull.getState())
        {
            using enum Seagull::State;

        case resting:
            entry.entryAnimation->animator.setTransition(randomSeagullIdle(entry), SeagullConstants::blendTime);
            break;

        case panicking:
            entry.entryAnimation->animator.setTransition(getAnimation(entry, "fly"), SeagullConstants::blendTime);
            break;

        case returning:
            entry.entryAnimation->animator.setTransition(getAnimation(entry, "glide"), SeagullConstants::blendTime);
            break;

        default:
            break;
        }
    }

    Transform &seagullTransform{entry.worldTransforms.front()};
    seagullTransform.setAbsoluteRotation(seagull.getRotation());
    seagullTransform.setAbsoluteTranslation(seagull.getPosition());

    entry.entryAnimation->animator.update(getSkeleton(entry), entry.primitives, deltaTime);
}