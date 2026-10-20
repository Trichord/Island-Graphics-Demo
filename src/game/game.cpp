#include "game/game.h"

#include <functional>
#include <glm/glm.hpp>
#include <memory>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "physics/physics_world.h"
#include "physics/player_state.h"
#include "scene/island_scene/island_scene.h"
#include <algorithm>
#include <thread>

Game::Game(const GraphicsSettings &settings, int windowWidth, int windowHeight,
           const std::function<void(const std::string &)> &updateProgress)
    : threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)},
      textureManager_{threadPool_, settings, updateProgress},
      renderer_{textureManager_, settings, windowWidth, windowHeight},
      scene_{std::make_unique<IslandScene>(textureManager_, "./res/scenes/island_scene.json", updateProgress)},
      audio_{*scene_, "./res/scenes/island_audio.json"}
{
    // Initialize scene dependent resources
    playerCamera_ = {scene_->getCameraDescription(), settings.renderWidth / static_cast<float>(settings.renderHeight)};
    renderer_.initializeResources(*scene_, playerCamera_);

    // Texture Manager defers loading of textures from file until all arrays have been pre-allocated from
    // Renderer::initializeResources. Safe to stop the pool now.
    threadPool_.stopAllThreads();

    updateProgress("Creating Collision Mesh... ");
    physics_.initializeScene(*scene_);
    updateProgress("Done\n");
}

void Game::resizeExternalResolution(int width, int height)
{
    renderer_.updateExternalResolution(width, height);
}

void Game::update(const UserInput &input, float time, float deltaTime)
{
    playerCamera_.addPitch(input.mouseDeltaY);
    playerCamera_.addYaw(input.mouseDeltaX);

    PlayerState state{updatePhysics(input, deltaTime)};
    scene_->physicsSync(physics_.getMovedObjects());
    scene_->onUpdate(state, time, deltaTime);
    updateAudio(state, deltaTime);

    renderer_.render(*scene_, playerCamera_, time, deltaTime);

    scene_->clear();
    physics_.clear();
}

PlayerState Game::updatePhysics(const UserInput &userInput, float deltaTime)
{
    physics_.update(userInput, playerCamera_.getFront(), deltaTime);

    // Synchronize camera with character position from the physics engine
    const glm::vec3 pos{physics_.getPlayerPosition()};
    playerCamera_.setPosition(pos);

    return {
        .position = pos,
        .front = playerCamera_.getFront(),
        .velocity = physics_.getPlayerVelocity(),
        .supported = physics_.playerIsSupported(),
        .grounded = physics_.playerIsGrounded(),
        .jumpTriggered = physics_.jumpEvent(),
        .landingTriggered = physics_.landingEvent(),
        .footstepTriggered = physics_.footstepEvent(),
        .running = userInput.sprintToggle,
        .crouched = userInput.crouchToggle,
    };
}

void Game::updateAudio(const PlayerState &state, float deltaTime)
{
    audio_.updateFromScene(scene_->getUpdatedEntries());
    audio_.playCharacterEffects(state);
    audio_.playTriggers(state, deltaTime);
}