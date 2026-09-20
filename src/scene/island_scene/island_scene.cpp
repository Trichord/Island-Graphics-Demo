#include "scene/island_scene/island_scene.h"

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <string_view>
#include <variant>
#include <vector>

#include "physics/collision_shapes.h"
#include "physics/player_state.h"
#include "scene/handle.h"
#include "scene/island_scene/seagull.h"
#include "scene/scene.h"
#include "utilities/overloaded.h"
#include "utilities/random.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

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
constexpr float panicDist{25.0f};
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
} // namespace

IslandScene::IslandScene(TextureManager &textureManager, const std::filesystem::path &jsonFile,
                         const std::function<void(const std::string &)> &updateProgress)
    : Scene(textureManager, jsonFile, updateProgress)
{
    // Cache original positions so that animated models can return or calculate updated positions
    Scene::Entry &butterfly{editEntry("butterfly")};
    butterflyOriginalPos_.reserve(butterfly.worldTransforms.size());

    for (const Transform &transform : butterfly.worldTransforms)
        butterflyOriginalPos_.emplace_back(transform.getPosition());

    // Set starting animations for first frame
    butterfly.entryAnimation->setAnimation("fly");

    // Seed the gulls with some randomness for some variety in their interactions
    for (std::size_t i{0}; i < seagullNames_.size(); ++i)
    {
        Scene::Entry &seagull{editEntry(seagullNames_[i])};

        seagulls_[i] = Seagull{seagull.worldTransforms.front(),
                               Seagull::State::resting,
                               SeagullConstants::panicDist + random::floatMS(-0.125f, 0.125f),
                               SeagullConstants::orbitHeight + random::floatMS(-3.0f, 3.0f),
                               SeagullConstants::orbitTime + random::floatMS(-2.0f, 8.0f),
                               SeagullConstants::orbitRadius + random::floatMS(-2.0f, 2.0f),
                               SeagullConstants::restTime + random::floatMS(0.0f, 0.2f)};

        std::string_view rndName{seagullIdles_[random::index(seagullIdles_.size())]};
        seagull.entryAnimation->setAnimation(rndName);

        // Simulate just a bit of a starting animation to desync
        seagull.entryAnimation->updateAnimation(seagull.primitives, random::floatMS(0.1f, 0.3f));
    }

    for (const Scene::Entry &entry : this->getAllSceneEntries())
    {
        if (!entry.collisionGeometry)
            continue;

        std::visit(overloaded{
                       [&](const collision::Mesh &mesh) { return; },
                       [&](const collision::Capsule &capsule) {
                           if (capsule.dynamic)
                               dynamicObjects_.emplace_back(entry.handle);
                       },
                       [&](const collision::Sphere &sphere) {
                           if (sphere.dynamic)
                               dynamicObjects_.emplace_back(entry.handle);
                       },
                       [&](const collision::ConvexHull &convexHull) {
                           if (convexHull.dynamic)
                               dynamicObjects_.emplace_back(entry.handle);
                       },
                   },
                   *entry.collisionGeometry);
    }
}

void IslandScene::onUpdate(const PlayerState &player, float time, float deltaTime)
{
    updateSpaceModel(editEntry("spaceModel"), deltaTime);
    updateButterfly(editEntry("butterfly"), time, deltaTime);

    for (std::size_t i{0}; i < seagullNames_.size(); ++i)
        updateSeagull(player.position, seagulls_[i], editEntry(seagullNames_[i]), deltaTime);
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
        const bool reversePattern{static_cast<int>(std::abs(originalPos.x)) % 2 == 1};
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

    butterfly.entryAnimation->updateAnimation(butterfly.primitives, deltaTime * Butterfly::animationTimeScale);
}

void IslandScene::updateSeagull(const glm::vec3 &playerPos, Seagull &seagull, Scene::Entry &entry, float deltaTime)
{
    const glm::vec3 &pos{seagull.getPosition()};
    float closest{glm::length2(playerPos - pos)};
    Handle closestHandle{0};
    std::size_t index{0};

    // Brute force check for closest dynamic since the number is small
    for (const Handle &handle : dynamicObjects_)
    {
        const Entry &entry{viewEntry(handle)};

        for (std::size_t i{0}; i < entry.worldTransforms.size(); ++i)
        {
            const float dist{glm::length2(entry.worldTransforms[i].getPosition() - pos)};
            if (dist < closest)
            {
                index = i;
                closestHandle = handle;
                closest = dist;
            }
        }
    }

    seagull.update(closestHandle ? viewEntry(closestHandle).worldTransforms[index].getPosition() : playerPos,
                   deltaTime);
    seagull.getPosition();

    if (seagull.stateChanged())
    {
        switch (seagull.getState())
        {
            using enum Seagull::State;

        case resting:
            entry.entryAnimation->setAnimationTransition(seagullIdles_[random::index(seagullIdles_.size())],
                                                         SeagullConstants::blendTime);
            break;

        case panicking:
            entry.entryAnimation->setAnimationTransition("fly", SeagullConstants::blendTime);
            break;

        case returning:
            entry.entryAnimation->setAnimationTransition("glide", SeagullConstants::blendTime);
            break;

        default:
            break;
        }
    }

    Transform &seagullTransform{entry.worldTransforms.front()};
    seagullTransform.setAbsoluteRotation(seagull.getRotation());
    seagullTransform.setAbsoluteTranslation(seagull.getPosition());
    entry.entryAnimation->updateAnimation(entry.primitives, deltaTime);
}